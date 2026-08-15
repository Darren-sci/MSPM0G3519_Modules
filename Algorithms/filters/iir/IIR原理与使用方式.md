# IIR 原理与使用方式

## 原理

IIR 同时利用当前与历史输入以及历史输出计算新输出。历史输出会重新参与后续计算，形成反馈，因此脉冲影响可以持续很久。代码把滤波器拆成若干二阶节，每节保存两次输入和两次输出历史，并包含三个前向系数与两个反馈系数。高阶滤波器通过多个二阶节依次级联实现，这比直接使用高阶系数更容易控制定点误差和数值稳定性。

关键函数 `IIR_processBiquad()` 将当前输入、四个历史状态和五个系数进行六十四位乘加，再把结果四舍五入并饱和到 Q15。得到输出后，函数按顺序更新输入及输出历史。`IIR_processSample()` 让数据依次经过全部二阶节，前一节输出作为后一节输入。输入和状态采用 Q15，系数采用 Q30，以容纳接近正负二的常用反馈系数。

## 使用方式

调用者先准备二阶节系数数组、等长的状态数组和一个 `IIR_Filter` 实例，再调用 `IIR_init()`。实时采样使用 `IIR_processSample()`，DMA 数据块使用 `IIR_processBlock()`，并允许输入输出共用缓冲区。重新开始独立测量、切换通道或量程时，调用 `IIR_reset()` 清除反馈历史。

ADC 数据应先扣除零偏、完成校准并转换到 Q15。不同通道可以共享系数，但必须各自保存状态。系数必须按照实际采样率和目标频率设计，改变采样率后要重新生成。当前实现使用常见分母符号，计算时会减去反馈项；复制其他库的系数前必须核对符号约定。初始化函数不会判断滤波器是否稳定，正式使用前应检查极点、阶跃响应、饱和及持续振荡。二阶节过多会增加六十四位运算时间，不宜直接放入高频中断。

## 常用系数实例

每一组花括号表示一个二阶节，成员顺序使用名称明确标出。直通和一阶平滑与采样率没有固定的赫兹指标；带截止频率的二阶低通只适用于标注的采样率。

```c
/* 直通，用于检查 IIR 处理链。 */
static const IIR_BiquadCoefficients iirPassThrough[1] = {{
    .b0 = IIR_Q30_ONE,
    .b1 = 0,
    .b2 = 0,
    .a1 = 0,
    .a2 = 0
}};

/* 一阶指数平滑低通：新输入占八分之一，历史输出占八分之七。 */
static const IIR_BiquadCoefficients iirSmoothing[1] = {{
    .b0 = 134217728,
    .b1 = 0,
    .b2 = 0,
    .a1 = -939524096,
    .a2 = 0
}};

/* 二阶 Butterworth 低通：采样率 10 kHz，截止频率 1 kHz。 */
static const IIR_BiquadCoefficients iirLowPass1kAt10k[1] = {{
    .b0 = 72429549,
    .b1 = 144859098,
    .b2 = 72429549,
    .a1 = -1227265970,
    .a2 = 443242341
}};

/* 缓慢直流漂移抑制，高频增益缩小一半以降低阶跃饱和风险。 */
static const IIR_BiquadCoefficients iirDcBlocker[1] = {{
    .b0 = 536870912,
    .b1 = -536870912,
    .b2 = 0,
    .a1 = -1068373115,
    .a2 = 0
}};
```

`iirLowPass1kAt10k` 与当前默认的 10 kframe/s 相匹配。如果调用 `ADCMulti_start()` 改成其他帧率，这组系数的实际截止频率会一起变化，正式使用时应重新生成系数。直流抑制器启动时可能产生瞬态，开始统计前应等待状态建立。

IIR 的瞬态或某个中间二阶节可能超过最终稳态幅度，应给 Q15 留出余量。满量程 ADC 信号不要直接映射到接近正负 32768，否则中间节饱和后即使后续幅度下降，失真也无法恢复。

## 示例一：在 main 中滤波一路 ADC

下面假设双极性信号被模拟前端偏置到 ADC 码值 2048。实际工程应分别测量各通道零偏，再完成量程和增益校准。

```c
#include "ti_msp_dl_config.h"
#include "Drivers/adc_multi.h"
#include "Algorithms/filters/iir/iir.h"

#define VIN_IIR_SECTION_COUNT  (1U)

static const IIR_BiquadCoefficients gVinCoefficients[1] = {{
    .b0 = 72429549,
    .b1 = 144859098,
    .b2 = 72429549,
    .a1 = -1227265970,
    .a2 = 443242341
}};
static IIR_BiquadState gVinState[VIN_IIR_SECTION_COUNT];
static IIR_Filter gVinFilter;
static int16_t gFilteredVin[ADC_MULTI_FRAME_COUNT];

static int16_t ADC_biasedCodeToQ15(uint16_t code, uint16_t zeroCode)
{
    /* 只映射到约半量程，为 IIR 瞬态和级联中间结果保留余量。 */
    int32_t value = ((int32_t) code - (int32_t) zeroCode) * 8;

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

    if (!IIR_init(&gVinFilter, gVinCoefficients,
        gVinState, VIN_IIR_SECTION_COUNT)) {
        while (1) {
        }
    }
    if (!ADCMulti_start(ADC_MULTI_DEFAULT_RATE_HZ)) {
        while (1) {
        }
    }

    while (1) {
        if (ADCMulti_getReadyBuffer(&frames, &frameCount)) {
            for (index = 0U; index < frameCount; index++) {
                int16_t vin = ADC_biasedCodeToQ15(
                    frames[index].vin, 2048U);
                gFilteredVin[index] =
                    IIR_processSample(&gVinFilter, vin);
            }

            ADCMulti_releaseBuffer(frames);
            /* 在这里使用 gFilteredVin 计算 RMS、频率或 FFT。 */
        }
    }
}
```

必须先处理完依赖原始帧的数据，再释放 ADC 缓冲区。滤波结果已经复制到 `gFilteredVin`，释放后仍然有效。IIR 状态不能在每个 DMA 块开始时复位，否则会破坏数据块之间的连续性。

## 示例二：四通道共享系数并独立保存状态

```c
#define COMMON_IIR_SECTIONS  (1U)

static const IIR_BiquadCoefficients gCommonIIR[1] = {{
    .b0 = 72429549,
    .b1 = 144859098,
    .b2 = 72429549,
    .a1 = -1227265970,
    .a2 = 443242341
}};
static IIR_BiquadState
    gIIRStates[ADC_MULTI_CHANNEL_COUNT][COMMON_IIR_SECTIONS];
static IIR_Filter gIIRFilters[ADC_MULTI_CHANNEL_COUNT];

typedef struct {
    int16_t vin;
    int16_t iin;
    int16_t vout;
    int16_t iout;
} IIR_FilteredFrame;

static IIR_FilteredFrame gIIRFrames[ADC_MULTI_FRAME_COUNT];

static bool IIRChannels_init(void)
{
    uint8_t channel;

    for (channel = 0U; channel < ADC_MULTI_CHANNEL_COUNT; channel++) {
        if (!IIR_init(&gIIRFilters[channel], gCommonIIR,
            gIIRStates[channel], COMMON_IIR_SECTIONS)) {
            return false;
        }
    }
    return true;
}

static void IIRChannels_process(
    const ADCMulti_Frame *input, uint16_t count)
{
    uint16_t i;

    for (i = 0U; i < count; i++) {
        gIIRFrames[i].vin = IIR_processSample(
            &gIIRFilters[ADC_MULTI_VIN],
            ADC_biasedCodeToQ15(input[i].vin, 2048U));
        gIIRFrames[i].iin = IIR_processSample(
            &gIIRFilters[ADC_MULTI_IIN],
            ADC_biasedCodeToQ15(input[i].iin, 2048U));
        gIIRFrames[i].vout = IIR_processSample(
            &gIIRFilters[ADC_MULTI_VOUT],
            ADC_biasedCodeToQ15(input[i].vout, 2048U));
        gIIRFrames[i].iout = IIR_processSample(
            &gIIRFilters[ADC_MULTI_IOUT],
            ADC_biasedCodeToQ15(input[i].iout, 2048U));
    }
}
```

系数数组可以共享，因为它只读；每个通道的反馈历史必须独立。滤波后使用有符号帧，避免将负 Q15 数据误认为很大的无符号 ADC 码值。如果四路模拟前端的带宽或零偏不同，应分别配置系数和零偏。

## 示例三：整块原地处理

```c
static int16_t gChannelData[ADC_MULTI_FRAME_COUNT];

/* 先把一路 ADC 原始码校准并转换到 gChannelData。 */
IIR_processBlock(&gVinFilter,
    gChannelData, gChannelData, ADC_MULTI_FRAME_COUNT);
```

原地处理不会破坏历史数据，因为 IIR 自己保存上一、上上次输入和输出。切换测量对象、改变系数或开始一轮互不相关的测量时，应调用 `IIR_reset()` 或重新调用 `IIR_init()`。仅仅进入下一个连续 DMA 数据块时不要复位。
