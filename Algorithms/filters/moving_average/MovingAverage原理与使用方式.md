# Moving Average 原理与使用方式

## 原理

滑动平均保留最近若干个采样，并输出这些采样的平均值。窗口越大，随机抖动越小，但波形变化会更迟缓，延迟也更明显。它适合平滑直流测量值、功率、温度、幅值和缓慢变化的控制量，不适合保留快速边沿或高频细节。

关键函数 `MovingAverage_processSample()` 使用环形状态数组保存窗口。窗口未满时，新值加入运行累加和，并用当前有效样本数求平均；窗口填满后，每次先从累加和中减去即将被覆盖的最旧值，再加入新值。这样无论窗口多大，每个采样都只进行固定次数的加减和一次除法，不需要重新遍历整个窗口。除法对正负数分别进行四舍五入，状态和运行累加和由滤波器实例持续保存。

## 使用方式

调用者准备一个 `MovingAverage_Filter` 和长度等于窗口的 `int16_t` 状态数组，再调用 `MovingAverage_init()`。实时处理使用 `MovingAverage_processSample()`，数据块处理使用 `MovingAverage_processBlock()`，输入输出允许共用数组。开始一轮无关的新测量或切换通道时调用 `MovingAverage_reset()`；连续 DMA 数据块之间不要复位。

## 窗口选择实例

```c
/* 不进行平滑，仅用于检查调用链。 */
#define MA_WINDOW_PASS_THROUGH  (1U)

/* 轻度平滑，延迟较小，适合显示和一般测量。 */
#define MA_WINDOW_FAST          (3U)

/* 常用折中，适合 ADC 小幅随机抖动。 */
#define MA_WINDOW_NORMAL        (5U)

/* 较强平滑，适合直流值、功率和慢控制量。 */
#define MA_WINDOW_SLOW          (16U)
```

窗口大小不是固定截止频率。实际平滑效果同时取决于 ADC 采样率；采样率改变后，同一个窗口对应的时间长度也会改变。窗口不要求是奇数，但选择二的整数次幂便于以后增加移位优化。

## 示例一：在 main 中平滑一路 ADC

ADC 原始码范围为 0～4095，可以安全转换为 `int16_t` 后直接进行滑动平均。输出仍然是 ADC 原始码尺度，之后可以继续换算电压或电流。

```c
#include "ti_msp_dl_config.h"
#include "Drivers/adc_multi.h"
#include "Algorithms/filters/moving_average/moving_average.h"

#define VIN_MA_WINDOW  (8U)

static int16_t gVinMAState[VIN_MA_WINDOW];
static MovingAverage_Filter gVinMAFilter;
static int16_t gSmoothedVin[ADC_MULTI_FRAME_COUNT];

int main(void)
{
    const ADCMulti_Frame *frames;
    uint16_t frameCount;
    uint16_t index;

    SYSCFG_DL_init();
    ADCMulti_init();

    if (!MovingAverage_init(
        &gVinMAFilter, gVinMAState, VIN_MA_WINDOW)) {
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
                gSmoothedVin[index] = MovingAverage_processSample(
                    &gVinMAFilter, (int16_t) frames[index].vin);
            }

            ADCMulti_releaseBuffer(frames);
            /* 在这里对 gSmoothedVin 进行显示或慢变量测量。 */
        }
    }
}
```

如果信号经过零偏扣除并变为负数，滑动平均仍可直接处理，因为输入、状态和输出都是 `int16_t`。处理完原始 ADC 帧后应及时释放 DMA 缓冲区。

## 示例二：四通道独立平滑

四个通道必须拥有独立状态，窗口大小可以相同，也可以根据电压、电流通道带宽分别配置。

```c
#define COMMON_MA_WINDOW  (5U)

static int16_t
    gMAStates[ADC_MULTI_CHANNEL_COUNT][COMMON_MA_WINDOW];
static MovingAverage_Filter gMAFilters[ADC_MULTI_CHANNEL_COUNT];

typedef struct {
    int16_t vin;
    int16_t iin;
    int16_t vout;
    int16_t iout;
} MA_FilteredFrame;

static MA_FilteredFrame gMAFrames[ADC_MULTI_FRAME_COUNT];

static bool MAChannels_init(void)
{
    uint8_t channel;

    for (channel = 0U; channel < ADC_MULTI_CHANNEL_COUNT; channel++) {
        if (!MovingAverage_init(&gMAFilters[channel],
            gMAStates[channel], COMMON_MA_WINDOW)) {
            return false;
        }
    }
    return true;
}

static void MAChannels_process(
    const ADCMulti_Frame *input, uint16_t count)
{
    uint16_t i;

    for (i = 0U; i < count; i++) {
        gMAFrames[i].vin = MovingAverage_processSample(
            &gMAFilters[ADC_MULTI_VIN], (int16_t) input[i].vin);
        gMAFrames[i].iin = MovingAverage_processSample(
            &gMAFilters[ADC_MULTI_IIN], (int16_t) input[i].iin);
        gMAFrames[i].vout = MovingAverage_processSample(
            &gMAFilters[ADC_MULTI_VOUT], (int16_t) input[i].vout);
        gMAFrames[i].iout = MovingAverage_processSample(
            &gMAFilters[ADC_MULTI_IOUT], (int16_t) input[i].iout);
    }
}
```

## 示例三：整块原地处理

```c
static int16_t gChannelData[ADC_MULTI_FRAME_COUNT];

/* 先把一路 ADC 数据写入 gChannelData。 */
MovingAverage_processBlock(&gVinMAFilter,
    gChannelData, gChannelData, ADC_MULTI_FRAME_COUNT);
```

滑动平均会削弱快速变化和周期信号的部分频率成分，因此不应在 FFT、相位、瞬态上升时间或波形失真分析前盲目使用。只想降低偶发尖峰时，中值滤波通常更合适。
