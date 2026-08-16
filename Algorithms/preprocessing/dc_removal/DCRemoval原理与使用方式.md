# DC Removal 原理与使用方式

## 原理

DC Removal 用于从一组已经校准的数据中分离直流中心和交流变化。自动处理接口先计算当前数据块的整数平均值，再从每个采样中减去这个平均值。返回的 `removedMean` 是被移除的直流分量，输出则围绕零变化，适合继续进行 FFT、交流 RMS、频率和纹波分析。

它与 ADC 校准中的零点修正不同。ADC 校准消除的是测量系统在真实零输入下产生的硬件偏差，并保留信号真实存在的直流量；DC Removal 主动移除当前信号的平均值，因此是根据后续算法选择使用的可选步骤。测量直流电压、平均电流或直流功率时不能丢弃 `removedMean`。

`DCRemoval_processInt16()` 和 `DCRemoval_processInt32()` 调用 Mean 模块取得块平均值，再调用对应的减法函数。减法使用更宽的中间类型，结果超出目标整数范围时进行饱和。`DCRemoval_subtractInt16()` 与 `DCRemoval_subtractInt32()` 不重新估计平均值，适合使用跨多个数据块得到的稳定直流估计。

## 使用方式

对单个数据块自动去直流时调用 `DCRemoval_processInt16()` 或 `DCRemoval_processInt32()`。输入与输出可以是同一缓冲区。已经通过 Mean 累加器获得稳定直流值时，使用 `DCRemoval_subtractInt16()` 或 `DCRemoval_subtractInt32()`。自动接口要求样本数量大于零；指定直流值的接口允许空数据块。

## 示例一：ADC 校准后原地去直流

下例把 `vin` 校准为双极性 Q15，再原地减去当前 512 帧的平均值。平均值单独保存，交流数据可以送入 FFT。

```c
#include "ti_msp_dl_config.h"
#include "Drivers/adc_multi.h"
#include "Algorithms/preprocessing/adc_calibration/adc_calibration.h"
#include "Algorithms/preprocessing/dc_removal/dc_removal.h"

static ADCCalibration_Config gVinCalibration;
static int16_t gVinQ15[ADC_MULTI_FRAME_COUNT];
static int32_t gVinDC;

int main(void)
{
    const ADCMulti_Frame *frames;
    uint16_t frameCount;
    uint16_t i;

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
            for (i = 0U; i < frameCount; i++) {
                gVinQ15[i] = ADCCalibration_applyToInt16(
                    &gVinCalibration, frames[i].vin);
            }
            ADCMulti_releaseBuffer(frames);

            if (DCRemoval_processInt16(
                gVinQ15, gVinQ15, frameCount, &gVinDC)) {
                /* gVinDC 是直流分量，gVinQ15 是去直流后的交流量。 */
            }
        }
    }
}
```

整数平均值经过四舍五入，去直流后数据的整数平均值通常为零，也可能因量化剩余接近零。不要为了强制精确为零而修改个别采样，否则会人为制造频谱成分。

## 示例二：保留直流量并计算交流 RMS

```c
int32_t dcValue;
uint32_t acRMS;

if (DCRemoval_processInt16(
    gVinQ15, gVinQ15, frameCount, &dcValue)) {
    RMS_calculate(gVinQ15, frameCount, &acRMS);
}
```

由于输入已经去直流，这里调用总 RMS 接口即可得到交流部分的 RMS。也可以不修改原数组，直接调用 `RMS_calculateAC()`；两种方式应根据后续是否还需要去直流波形来选择。

## 示例三：使用多个 DMA 块估计稳定直流值

逐块减去各自平均值会让低频信号被不同程度地削弱，还可能在块边界产生跳变。可以先用 Mean 累加器统计多个数据块，再把得到的固定直流值用于后续数据。

```c
static Mean_Accumulator gDCMean;
static int32_t gStableDC;

Mean_reset(&gDCMean);

/* 对若干个已经校准的块重复调用。 */
Mean_addBlockInt16(&gDCMean, gVinQ15, frameCount);

/* 收集到足够数据后取得稳定直流值。 */
if (Mean_get(&gDCMean, &gStableDC)) {
    DCRemoval_subtractInt16(
        gVinQ15, gVinQ15, frameCount, gStableDC);
}
```

这种方式适合直流中心稳定、交流频率较低或需要保持块间连续性的情况。如果直流中心会缓慢漂移，可以按固定时间重新估计，或后续实现专门的低速直流跟踪器。

## 示例四：四通道分别去直流

四路数据需要分别计算平均值。不要把电压和电流通道混在同一个数组中求平均。

```c
static int16_t gChannelQ15
    [ADC_MULTI_CHANNEL_COUNT][ADC_MULTI_FRAME_COUNT];
static int32_t gChannelDC[ADC_MULTI_CHANNEL_COUNT];

for (channel = 0U; channel < ADC_MULTI_CHANNEL_COUNT; channel++) {
    DCRemoval_processInt16(
        gChannelQ15[channel],
        gChannelQ15[channel],
        ADC_MULTI_FRAME_COUNT,
        &gChannelDC[channel]);
}
```

## 注意事项

- 每块独立去平均相当于抑制非常低的频率，数据块越短，影响越明显。
- 数据块没有覆盖完整周期时，块平均值可能包含部分交流分量，减去后会改变波形。
- FFT 前通常需要去直流，但如果需要观察零频分量，应保留原数据或记录 `removedMean`。
- 直流电压、电流、平均功率等测量必须使用校准后的原始物理量，不能只看去直流结果。
- 输入接近整数边界时，减去较大的反向直流量可能饱和；饱和会造成不可恢复的失真。
- `dc_removal.c` 依赖 Mean 模块，工程构建时必须同时编译 `mean.c`。
