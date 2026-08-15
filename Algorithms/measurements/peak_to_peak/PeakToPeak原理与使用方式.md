# Peak-to-Peak 原理与使用方式

## 原理

峰峰值表示统计区间内最大值与最小值之间的跨度，适合描述周期波形摆幅、纹波、噪声包络和 ADC 动态范围。算法只需遍历数据并不断更新最小值和最大值，不需要乘法、除法或浮点运算，因此计算量很小。

`PeakToPeak_Accumulator` 保存当前最小值、最大值和样本数量。`PeakToPeak_reset()` 把最小值初始化为最大的有符号数，把最大值初始化为最小的有符号数，使第一个真实采样能同时更新两者。`PeakToPeak_get()` 使用六十四位有符号差值计算跨度，再转换成 `uint32_t`，因此即使输入覆盖完整的 `int32_t` 范围也不会发生有符号溢出。

## 使用方式

单个数组使用 `PeakToPeak_calculateInt16()` 或 `PeakToPeak_calculateInt32()`。跨 DMA 块统计时先调用 `PeakToPeak_reset()`，再加入单点或数据块，最后使用 `PeakToPeak_get()`。返回的 `PeakToPeak_Result` 同时包含最小值、最大值和非负跨度。没有样本时接口返回 `false`。

## 示例一：测量一路 ADC 的最小值、最大值和峰峰值

```c
#include "ti_msp_dl_config.h"
#include "Drivers/adc_multi.h"
#include "Algorithms/measurements/peak_to_peak/peak_to_peak.h"

static int16_t gVinCodes[ADC_MULTI_FRAME_COUNT];
static PeakToPeak_Result gVinPeakToPeak;

int main(void)
{
    const ADCMulti_Frame *frames;
    uint16_t frameCount;
    uint16_t index;

    SYSCFG_DL_init();
    ADCMulti_init();
    if (!ADCMulti_start(ADC_MULTI_DEFAULT_RATE_HZ)) {
        while (1) {
        }
    }

    while (1) {
        if (ADCMulti_getReadyBuffer(&frames, &frameCount)) {
            for (index = 0U; index < frameCount; index++) {
                gVinCodes[index] = (int16_t) frames[index].vin;
            }
            ADCMulti_releaseBuffer(frames);

            if (PeakToPeak_calculateInt16(
                gVinCodes, frameCount, &gVinPeakToPeak)) {
                /* minimum、maximum、span 都是 ADC 原始码尺度。 */
            }
        }
    }
}
```

若要得到毫伏或毫安，推荐先逐点校准成物理量，再使用 `PeakToPeak_calculateInt32()`。不能只把原始码峰峰值乘以单一比例，因为正负两侧可能使用不同校准增益。

## 示例二：测量校准后的外部电压摆幅

```c
static int32_t gVinMillivolts[ADC_MULTI_FRAME_COUNT];
static PeakToPeak_Result gVinMillivoltResult;

for (index = 0U; index < frameCount; index++) {
    gVinMillivolts[index] = ADCCalibration_apply(
        &gVinCalibration, frames[index].vin);
}
ADCMulti_releaseBuffer(frames);

PeakToPeak_calculateInt32(
    gVinMillivolts, frameCount, &gVinMillivoltResult);
```

此时 `minimum` 和 `maximum` 是有符号毫伏，`span` 是非负毫伏。对于反相模拟前端，校准模块已经处理极性，峰峰值模块不需要特殊修改。

## 示例三：四通道跨多个 DMA 块统计

```c
static PeakToPeak_Accumulator
    gPeakAcc[ADC_MULTI_CHANNEL_COUNT];
static PeakToPeak_Result
    gPeakResult[ADC_MULTI_CHANNEL_COUNT];

static void ChannelPeak_reset(void)
{
    uint8_t channel;

    for (channel = 0U; channel < ADC_MULTI_CHANNEL_COUNT; channel++) {
        PeakToPeak_reset(&gPeakAcc[channel]);
    }
}

static bool ChannelPeak_addFrames(
    const ADCMulti_Frame *frames, uint16_t count)
{
    uint16_t i;

    for (i = 0U; i < count; i++) {
        if (!PeakToPeak_addSample(
                &gPeakAcc[ADC_MULTI_VIN], frames[i].vin) ||
            !PeakToPeak_addSample(
                &gPeakAcc[ADC_MULTI_IIN], frames[i].iin) ||
            !PeakToPeak_addSample(
                &gPeakAcc[ADC_MULTI_VOUT], frames[i].vout) ||
            !PeakToPeak_addSample(
                &gPeakAcc[ADC_MULTI_IOUT], frames[i].iout)) {
            return false;
        }
    }
    return true;
}

static bool ChannelPeak_get(void)
{
    uint8_t channel;

    for (channel = 0U; channel < ADC_MULTI_CHANNEL_COUNT; channel++) {
        if (!PeakToPeak_get(
            &gPeakAcc[channel], &gPeakResult[channel])) {
            return false;
        }
    }
    return true;
}
```

统计时间越长，遇到偶发毛刺的概率越大，峰峰值可能只反映一个异常采样。需要测量稳定周期波形时，应选择明确的时间窗口，必要时先进行中值滤波或连续多块测量后取稳定结果。

## 示例四：同时计算平均值、RMS 和峰峰值

```c
int32_t mean;
uint32_t rms;
PeakToPeak_Result peak;

Mean_calculateInt16(gVinQ15, frameCount, &mean);
RMS_calculate(gVinQ15, frameCount, &rms);
PeakToPeak_calculateInt16(gVinQ15, frameCount, &peak);
```

三个接口会分别遍历数组，写法清晰，适合当前 512 帧数据块。以后若高采样率下 CPU 时间不足，可以增加一次遍历同时统计多个指标的综合模块。

## 注意事项

- 峰峰值对单点毛刺非常敏感，应结合中值滤波或异常点检测判断结果可信度。
- 低采样率可能错过模拟波形真正的峰值，数字峰峰值会偏小。
- 统计区间没有覆盖完整周期时，最大值和最小值可能不完整。
- 削顶时最大值或最小值会长时间贴近 ADC 边界，应同时报告超量程状态。
- 峰峰值只反映极值跨度，不代表信号能量；功率相关测量应使用 RMS。
- 累加器开始使用前必须调用 `PeakToPeak_reset()`。
