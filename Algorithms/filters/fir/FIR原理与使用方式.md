# FIR 原理与使用方式

## 原理

FIR 使用当前采样和有限数量的历史采样计算输出。“抽头”就是参与一次计算的采样位置，每个抽头对应一个系数。抽头越多，越容易获得更陡的频率响应，但乘加次数、状态内存和处理时间也会增加。系数决定滤波器是低通、高通、带通还是带阻；本模块只负责通用运算，并不固定滤波类型。

关键函数 `FIR_processSample()` 先把新输入写入环形状态缓冲区，再从当前位置反向读取当前及历史采样，使第零个系数始终对应当前输入。所有系数与采样的乘积使用六十四位变量累加，避免多抽头时发生三十二位溢出。计算完成后推进写入位置，到达数组末尾便回到开头，因此不需要搬移历史数据。最后将累加结果从 Q15 乘积尺度转换回 Q15，进行对称四舍五入，并把超出范围的结果饱和到有符号十六位边界。

## 使用方式

调用者先准备只读 Q15 系数数组、与抽头数等长的状态数组和一个 `FIR_Filter` 实例，再调用 `FIR_init()`。单点实时处理使用 `FIR_processSample()`；处理 ADC 数据块使用 `FIR_processBlock()`，输入与输出可以是同一数组。重新开始采集、切换通道或切换量程时，可调用 `FIR_reset()` 清除旧历史。

ADC 原始值不能直接作为 Q15 交流量，应先完成零偏扣除、校准和缩放。多个通道可以共享系数，但必须各自拥有状态数组。系数与采样率配套，改变采样率后应重新设计系数。必须保证系数和状态在滤波期间持续有效，也不要在中断中使用过多抽头；Cortex-M0+ 的六十四位运算可靠但速度较慢。使用前还应验证增益相位以及输出溢出情况。

## 常用抽头实例

下列系数均为 Q15 整数，可以直接交给 `FIR_init()`。这些短滤波器适合功能验证和一般平滑，不适合替代经过指标设计的窄带或陡峭滤波器。

```c
/* 近似直通，用于检查处理链路。 */
static const int16_t firPassThrough[1] = {
    32767
};

/* 三抽头加权平滑低通：0.25、0.50、0.25。 */
static const int16_t firLowPass3[3] = {
    8192, 16384, 8192
};

/* 五抽头平滑低通，比三抽头更平滑，但延迟增加到两个采样周期。 */
static const int16_t firLowPass5[5] = {
    2048, 8192, 12288, 8192, 2048
};

/* 三抽头高通：系数和为零，可去除直流及缓慢漂移。 */
static const int16_t firHighPass3[3] = {
    -8192, 16384, -8192
};
```

低通系数之和为 32768，因此直流增益接近一。高通系数之和为零，因此恒定输入最终输出为零。抽头数、采样率和目标截止频率共同决定实际效果；50 Hz 陷波、带通等滤波器不能脱离采样率直接套用固定系数。

## 示例一：滤波一路 ADC 数据

假设模拟前端把双极性信号偏置在 ADC 码值 2048 附近。先扣除零偏，再乘以 16 转为 Q15。实际零偏应使用校准结果，不应长期写死为 2048。

```c
#include "ti_msp_dl_config.h"
#include "Drivers/adc_multi.h"
#include "Algorithms/filters/fir/fir.h"

#define VIN_FIR_TAP_COUNT  (5U)

static const int16_t gVinCoefficients[VIN_FIR_TAP_COUNT] = {
    2048, 8192, 12288, 8192, 2048
};
static int16_t gVinState[VIN_FIR_TAP_COUNT];
static FIR_Filter gVinFilter;
static int16_t gFilteredVin[ADC_MULTI_FRAME_COUNT];

static int16_t ADC_biasedCodeToQ15(uint16_t code, uint16_t zeroCode)
{
    int32_t value = ((int32_t) code - (int32_t) zeroCode) * 16;

    if (value > INT16_MAX) {
        value = INT16_MAX;
    } else if (value < INT16_MIN) {
        value = INT16_MIN;
    }
    return (int16_t) value;
}

int main(void)
{
    const ADCMulti_Frame *frames;
    uint16_t frameCount;
    uint16_t index;

    SYSCFG_DL_init();
    ADCMulti_init();
    FIR_init(&gVinFilter, gVinCoefficients,
        gVinState, VIN_FIR_TAP_COUNT);
    ADCMulti_start(ADC_MULTI_DEFAULT_RATE_HZ);

    while (1) {
        if (ADCMulti_getReadyBuffer(&frames, &frameCount)) {
            for (index = 0U; index < frameCount; index++) {
                int16_t vin = ADC_biasedCodeToQ15(
                    frames[index].vin, 2048U);
                gFilteredVin[index] =
                    FIR_processSample(&gVinFilter, vin);
            }

            ADCMulti_releaseBuffer(frames);
            /* 在这里使用 gFilteredVin 计算 RMS、频率或 FFT。 */
        }
    }
}
```

## 示例二：四通道使用同一组抽头

四个通道可以共享只读系数，但每路必须保存自己的历史状态。下面采用逐帧处理，避免先拆分成四个大型输入数组。

```c
#define FIR_TAPS  (3U)

static const int16_t gCommonCoefficients[FIR_TAPS] = {
    8192, 16384, 8192
};
static int16_t gStates[ADC_MULTI_CHANNEL_COUNT][FIR_TAPS];
static FIR_Filter gFilters[ADC_MULTI_CHANNEL_COUNT];

typedef struct {
    int16_t vin;
    int16_t iin;
    int16_t vout;
    int16_t iout;
} FIR_FilteredFrame;

static FIR_FilteredFrame gFilteredFrames[ADC_MULTI_FRAME_COUNT];

static void Filters_init(void)
{
    uint8_t channel;

    for (channel = 0U; channel < ADC_MULTI_CHANNEL_COUNT; channel++) {
        FIR_init(&gFilters[channel], gCommonCoefficients,
            gStates[channel], FIR_TAPS);
    }
}

static void Filters_processFrames(
    const ADCMulti_Frame *input, uint16_t count)
{
    uint16_t i;

    for (i = 0U; i < count; i++) {
        gFilteredFrames[i].vin = FIR_processSample(
            &gFilters[ADC_MULTI_VIN],
            ADC_biasedCodeToQ15(input[i].vin, 2048U));
        gFilteredFrames[i].iin = FIR_processSample(
            &gFilters[ADC_MULTI_IIN],
            ADC_biasedCodeToQ15(input[i].iin, 2048U));
        gFilteredFrames[i].vout = FIR_processSample(
            &gFilters[ADC_MULTI_VOUT],
            ADC_biasedCodeToQ15(input[i].vout, 2048U));
        gFilteredFrames[i].iout = FIR_processSample(
            &gFilters[ADC_MULTI_IOUT],
            ADC_biasedCodeToQ15(input[i].iout, 2048U));
    }
}
```

这里单独定义了成员为 `int16_t` 的滤波帧，避免把负的 Q15 输出误当作很大的无符号 ADC 码值。`ADC_biasedCodeToQ15()` 使用示例一中的实现，每个通道的实际零偏应分别校准。

## 示例三：整块原地滤波

如果前处理已经把某一路 ADC 数据转换到独立的 Q15 数组，可以直接整块处理。输入和输出允许使用同一数组。

```c
static int16_t gChannelData[ADC_MULTI_FRAME_COUNT];

/* 先把某一路 ADC 数据校准并写入 gChannelData。 */
FIR_processBlock(&gVinFilter,
    gChannelData, gChannelData, ADC_MULTI_FRAME_COUNT);
```

滤波器状态会跨数据块保留，所以相邻 DMA 块之间是连续的。只有在重新开始一次独立测量、改变通道来源或希望丢弃旧历史时才调用 `FIR_reset()`。如果更换抽头数量或系数地址，应重新调用 `FIR_init()`。
