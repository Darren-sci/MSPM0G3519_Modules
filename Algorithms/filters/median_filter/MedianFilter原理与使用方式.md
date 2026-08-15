# Median Filter 原理与使用方式

## 原理

中值滤波保存最近若干个采样，将它们从小到大排列，并输出位于中间的值。它不是加权求和，而是从已有采样中选择一个结果，因此特别适合去除单点毛刺、通信干扰和偶发 ADC 异常值。对于持续的随机噪声，它通常不如滑动平均、FIR 或 IIR 平滑。

窗口必须是大于零的奇数，才能得到唯一的中间位置。关键函数 `MedianFilter_processSample()` 先把新值写入环形状态数组，再把当前有效样本复制到 `scratch` 临时数组。`MedianFilter_sort()` 使用插入排序排列临时数组，最后返回中间元素。排序只修改临时数组，不会破坏用于下一次处理的历史状态。窗口填满前仅使用已经收到的采样，避免把初始化的零误认为真实数据。

## 使用方式

调用者准备一个 `MedianFilter_Filter`、一个状态数组和一个等长临时数组，再调用 `MedianFilter_init()`。单点处理使用 `MedianFilter_processSample()`，整块处理使用 `MedianFilter_processBlock()`，输入输出允许共用缓冲区。重新开始独立测量或切换信号来源时使用 `MedianFilter_reset()`。

## 窗口选择实例

```c
/* 三点中值：可去除三个连续样本中的一个孤立异常值。 */
#define MEDIAN_WINDOW_FAST    (3U)

/* 五点中值：抗连续毛刺能力更强，但延迟和排序开销增加。 */
#define MEDIAN_WINDOW_NORMAL  (5U)

/* 七点中值：只建议用于较慢数据，不宜在高速采样中滥用。 */
#define MEDIAN_WINDOW_STRONG  (7U)
```

通常从三点窗口开始。窗口越大，越容易抹掉窄脉冲、尖峰波形和真实快速变化；如果异常点连续占据窗口的一半以上，中值滤波也无法正确恢复原信号。

## 示例一：在 main 中处理一路 ADC 毛刺

ADC 原始码 0～4095 可以直接转换为 `int16_t`。下例使用三点中值去除 `vin` 中孤立的异常采样，输出仍保持 ADC 原始码尺度。

```c
#include "ti_msp_dl_config.h"
#include "Drivers/adc_multi.h"
#include "Algorithms/filters/median_filter/median_filter.h"

#define VIN_MEDIAN_WINDOW  (3U)

static int16_t gVinMedianState[VIN_MEDIAN_WINDOW];
static int16_t gVinMedianScratch[VIN_MEDIAN_WINDOW];
static MedianFilter_Filter gVinMedianFilter;
static int16_t gMedianVin[ADC_MULTI_FRAME_COUNT];

int main(void)
{
    const ADCMulti_Frame *frames;
    uint16_t frameCount;
    uint16_t index;

    SYSCFG_DL_init();
    ADCMulti_init();

    if (!MedianFilter_init(&gVinMedianFilter,
        gVinMedianState, gVinMedianScratch, VIN_MEDIAN_WINDOW)) {
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
                gMedianVin[index] = MedianFilter_processSample(
                    &gVinMedianFilter, (int16_t) frames[index].vin);
            }

            ADCMulti_releaseBuffer(frames);
            /* 在这里使用 gMedianVin 继续测量或显示。 */
        }
    }
}
```

中值滤波不改变数据单位，可以在 ADC 零偏和增益校准前处理原始码，也可以在转换为有符号 Q15 后处理。若异常值判断依赖物理量阈值，应先完成校准。

## 示例二：四通道独立处理中值

每个通道都需要自己的状态和临时排序数组。临时数组在处理过程中会被改写，不能由同时工作的通道共享。

```c
#define COMMON_MEDIAN_WINDOW  (3U)

static int16_t gMedianStates
    [ADC_MULTI_CHANNEL_COUNT][COMMON_MEDIAN_WINDOW];
static int16_t gMedianScratch
    [ADC_MULTI_CHANNEL_COUNT][COMMON_MEDIAN_WINDOW];
static MedianFilter_Filter gMedianFilters[ADC_MULTI_CHANNEL_COUNT];

typedef struct {
    int16_t vin;
    int16_t iin;
    int16_t vout;
    int16_t iout;
} Median_FilteredFrame;

static Median_FilteredFrame gMedianFrames[ADC_MULTI_FRAME_COUNT];

static bool MedianChannels_init(void)
{
    uint8_t channel;

    for (channel = 0U; channel < ADC_MULTI_CHANNEL_COUNT; channel++) {
        if (!MedianFilter_init(&gMedianFilters[channel],
            gMedianStates[channel], gMedianScratch[channel],
            COMMON_MEDIAN_WINDOW)) {
            return false;
        }
    }
    return true;
}

static void MedianChannels_process(
    const ADCMulti_Frame *input, uint16_t count)
{
    uint16_t i;

    for (i = 0U; i < count; i++) {
        gMedianFrames[i].vin = MedianFilter_processSample(
            &gMedianFilters[ADC_MULTI_VIN], (int16_t) input[i].vin);
        gMedianFrames[i].iin = MedianFilter_processSample(
            &gMedianFilters[ADC_MULTI_IIN], (int16_t) input[i].iin);
        gMedianFrames[i].vout = MedianFilter_processSample(
            &gMedianFilters[ADC_MULTI_VOUT], (int16_t) input[i].vout);
        gMedianFrames[i].iout = MedianFilter_processSample(
            &gMedianFilters[ADC_MULTI_IOUT], (int16_t) input[i].iout);
    }
}
```

## 示例三：整块原地处理

```c
static int16_t gChannelData[ADC_MULTI_FRAME_COUNT];

/* 先把一路 ADC 数据写入 gChannelData。 */
MedianFilter_processBlock(&gVinMedianFilter,
    gChannelData, gChannelData, ADC_MULTI_FRAME_COUNT);
```

中值滤波需要复制并排序窗口，计算量随窗口明显增加，推荐只使用三点或五点窗口。它会改变尖脉冲、方波边沿和窄峰值，进行峰值检测、上升时间测量或故障脉冲捕获时应谨慎启用。
