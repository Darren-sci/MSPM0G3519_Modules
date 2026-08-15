# Frequency 原理与使用方式

## 原理

频率模块通过连续检测同方向的上升沿获得周期。输入先与阈值比较，只有信号降到阈值减滞回以下才重新准备下一次上升沿；越过阈值后还要继续达到阈值加滞回，边沿才会被确认。这样可以避免阈值附近的小幅噪声被误认为多个周期。

`Frequency_processSample()` 在相邻采样之间对阈值位置做线性插值，并用 Q16.16 采样点记录边沿时刻，因此周期不局限于整数采样点。相邻确认上升沿之差加入周期累加和，`Frequency_getResult()` 求平均周期，再结合实际采样率输出毫赫兹。估计器保存前一个采样和全局采样序号，所以数据可以跨 DMA 块连续加入。

## 使用方式

先启动 ADC 并取得实际帧率，再调用 `Frequency_init()`。之后对每个连续数据块调用 `Frequency_processBlock()`，有完整周期后使用 `Frequency_getResult()`。开始无关测量或切换信号来源时调用 `Frequency_reset()`。只测一个数组时可使用 `Frequency_calculate()`，但数组中至少需要两个确认上升沿。

## 示例一：在 main 中连续测量一路频率

下例把双极性信号校准为 Q15，使用零作为阈值、512 作为滞回宽度。滞回应大于阈值附近的噪声，但明显小于信号幅度。

```c
#include "ti_msp_dl_config.h"
#include "Drivers/adc_multi.h"
#include "Algorithms/preprocessing/adc_calibration/adc_calibration.h"
#include "Algorithms/measurements/frequency/frequency.h"

static ADCCalibration_Config gVinCalibration;
static Frequency_Estimator gVinFrequency;
static Frequency_Result gVinFrequencyResult;

int main(void)
{
    const ADCMulti_Frame *frames;
    uint16_t frameCount;
    uint16_t i;

    SYSCFG_DL_init();
    ADCMulti_init();
    ADCCalibration_initBipolarQ15(
        &gVinCalibration, 80U, 2048U, 4015U);

    if (!ADCMulti_start(ADC_MULTI_DEFAULT_RATE_HZ) ||
        !Frequency_init(&gVinFrequency,
            ADCMulti_getActualFrameRate(), 0, 512U)) {
        while (1) {
        }
    }

    while (1) {
        if (ADCMulti_getReadyBuffer(&frames, &frameCount)) {
            for (i = 0U; i < frameCount; i++) {
                int16_t sample = ADCCalibration_applyToInt16(
                    &gVinCalibration, frames[i].vin);
                Frequency_processSample(&gVinFrequency, sample);
            }
            ADCMulti_releaseBuffer(frames);

            if (Frequency_getResult(
                &gVinFrequency, &gVinFrequencyResult)) {
                uint32_t integerHz =
                    gVinFrequencyResult.frequencyMilliHz / 1000U;
                uint32_t decimalMilliHz =
                    gVinFrequencyResult.frequencyMilliHz % 1000U;
                /* 显示 integerHz.decimalMilliHz Hz。 */
            }
        }
    }
}
```

当前结果会平均估计器复位以来的全部周期，显示会越来越稳定但响应越来越慢。需要固定刷新速度时，可以显示结果后调用 `Frequency_reset()`，或每累计固定数量的数据块重新开始。

## 示例二：测量单个已整理的数据块

```c
Frequency_Result result;

if (Frequency_calculate(gFilteredVin, frameCount,
    ADCMulti_getActualFrameRate(), 0, 512U, &result)) {
    /* result.frequencyMilliHz 为毫赫兹。 */
}
```

滤波可以减少噪声误触发，但低通滤波会产生延迟。只测单路频率时固定延迟不会改变周期；后续做相位测量时，两路必须使用相同延迟的处理链。

## 阈值与滞回选择

- 双极性且已经去零偏的波形通常使用零阈值。
- 单极性方波可以使用高、低电平中点作为阈值。
- 波形带直流偏置时，可以先用平均值估计中心阈值。
- 滞回太小会重复触发，太大可能使小信号永远无法确认边沿。
- 正弦、三角波和方波均可测量，但阈值附近斜率越小，噪声造成的时间抖动越明显。

## 注意事项

- 频率计算必须使用 `ADCMulti_getActualFrameRate()`，不能假定请求帧率绝对准确。
- 被测频率接近采样率一半时，每周期采样太少，阈值插值也无法保证可靠。
- 数据块内少于两个上升沿时，单块接口会返回 `false`，流式接口可等待后续块。
- 削顶、严重谐波或一个周期内多次穿越阈值会造成错误计数。
- 输入来源或采样率改变后必须重新初始化估计器。
