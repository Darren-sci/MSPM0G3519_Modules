# RMS 原理与使用方式

## 原理

RMS 表示信号产生等效功率或热效应的有效幅度。计算过程先对每个采样平方，使正负半周都产生正贡献，再对平方结果求平均，最后进行平方根运算。总 RMS 包含直流和交流成分；交流 RMS 会先减去数据块平均值，只反映围绕直流中心变化的部分。

`RMS_Accumulator` 使用六十四位无符号整数保存平方和，适合跨多个 DMA 块计算 `int16_t` 数据的总 RMS。`RMS_get()` 使用整数平方根，并直接根据真实平方和判断最终舍入边界，避免先舍入均方值再开方造成双重误差。`RMS_calculateAC()` 先计算整数块平均值，再累计每个采样与平均值之差的平方，因此它只提供一次性数据块接口。

## 使用方式

一个数组的总 RMS 使用 `RMS_calculate()`，交流 RMS 使用 `RMS_calculateAC()`。跨数据块统计总 RMS 时，先调用 `RMS_reset()`，再重复加入单点或数据块，最后调用 `RMS_get()`。结果类型为 `uint32_t`，因为输入 `-32768` 的绝对幅度是 32768，无法由正的 `int16_t` 表示。

## 示例一：校准后计算一路总 RMS 与交流 RMS

下面先把双极性 ADC 数据校准为 Q15，再分别计算总 RMS 和去直流后的交流 RMS。

```c
#include "ti_msp_dl_config.h"
#include "Drivers/adc_multi.h"
#include "Algorithms/preprocessing/adc_calibration/adc_calibration.h"
#include "Algorithms/measurements/rms/rms.h"

static ADCCalibration_Config gVinCalibration;
static int16_t gVinQ15[ADC_MULTI_FRAME_COUNT];
static uint32_t gVinTotalRMS;
static uint32_t gVinACRMS;
static int32_t gVinMean;

int main(void)
{
    const ADCMulti_Frame *frames;
    uint16_t frameCount;
    uint16_t index;

    SYSCFG_DL_init();
    ADCMulti_init();
    ADCCalibration_initBipolarQ15(
        &gVinCalibration, 80U, 2048U, 4015U);

    if (!ADCMulti_start(ADC_MULTI_DEFAULT_RATE_HZ)) {
        while (1) {
        }
    }

    while (1) {
        if (ADCMulti_getReadyBuffer(&frames, &frameCount)) {
            for (index = 0U; index < frameCount; index++) {
                gVinQ15[index] = ADCCalibration_applyToInt16(
                    &gVinCalibration, frames[index].vin);
            }
            ADCMulti_releaseBuffer(frames);

            RMS_calculate(gVinQ15, frameCount, &gVinTotalRMS);
            RMS_calculateAC(gVinQ15, frameCount,
                &gVinACRMS, &gVinMean);
        }
    }
}
```

如果 Q15 满量程对应实际 12 V，物理量换算应在 RMS 之后按相同比例进行。不要先把每个采样转换成低精度整数伏特，否则会损失小信号分辨率。

## 示例二：对滤波结果计算 RMS

```c
/* gFilteredVin 是 FIR 或 IIR 输出的连续 Q15 数据。 */
uint32_t filteredRMS;

if (RMS_calculate(gFilteredVin, frameCount, &filteredRMS)) {
    /* filteredRMS 与 gFilteredVin 使用相同的 Q15 尺度。 */
}
```

滤波会改变噪声和谐波能量，因此“原始 RMS”和“滤波后 RMS”含义不同。功率或失真测量应根据题目带宽要求选择滤波器，不能只为了数值稳定随意平滑。

## 示例三：四通道跨 DMA 块统计总 RMS

```c
static RMS_Accumulator gRMSAcc[ADC_MULTI_CHANNEL_COUNT];
static uint32_t gChannelRMS[ADC_MULTI_CHANNEL_COUNT];

static void ChannelRMS_reset(void)
{
    uint8_t channel;

    for (channel = 0U; channel < ADC_MULTI_CHANNEL_COUNT; channel++) {
        RMS_reset(&gRMSAcc[channel]);
    }
}

static bool ChannelRMS_addFrames(
    const ADCMulti_Frame *frames, uint16_t count)
{
    uint16_t i;

    for (i = 0U; i < count; i++) {
        if (!RMS_addSample(
                &gRMSAcc[ADC_MULTI_VIN], (int16_t) frames[i].vin) ||
            !RMS_addSample(
                &gRMSAcc[ADC_MULTI_IIN], (int16_t) frames[i].iin) ||
            !RMS_addSample(
                &gRMSAcc[ADC_MULTI_VOUT], (int16_t) frames[i].vout) ||
            !RMS_addSample(
                &gRMSAcc[ADC_MULTI_IOUT], (int16_t) frames[i].iout)) {
            return false;
        }
    }
    return true;
}

static bool ChannelRMS_get(void)
{
    uint8_t channel;

    for (channel = 0U; channel < ADC_MULTI_CHANNEL_COUNT; channel++) {
        if (!RMS_get(&gRMSAcc[channel], &gChannelRMS[channel])) {
            return false;
        }
    }
    return true;
}
```

这个跨块示例直接处理非负 ADC 原始码，所以得到的是包含 ADC 偏置的总 RMS。测量双极性交流幅度时，应先逐点校准或扣除零偏，再加入累加器。交流 RMS 需要正确估计统计区间的平均值，目前应使用 `RMS_calculateAC()` 对已经整理好的完整数据块计算。

## 注意事项

- 总 RMS 包含直流，交流 RMS 会去除当前数据块的平均值，两者不能混用。
- 数据块过短或没有覆盖完整周期时，周期信号 RMS 会随截取位置波动。
- 输入削顶后 RMS 仍可能看似稳定，必须结合最小值、最大值和超量程检测。
- Q15 的 32768 只表示算法尺度，不自动等于伏特或安培，必须结合校准比例换算。
- `RMS_Accumulator` 使用前必须复位，统计完成后按所需刷新周期重新开始。
- 平方和采用整数计算，不会产生浮点依赖，但整数平方根仍比均值和峰峰值耗时。
