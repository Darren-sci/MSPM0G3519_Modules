# Mean 原理与使用方式

## 原理

平均值反映一组采样的直流中心。对 ADC 原始码求平均可以观察零点和偏置；对校准后的 mV、mA 数据求平均，可以得到平均电压或平均电流；对带正负号的交流数据求平均，结果应接近零。平均值也常用于去直流、稳定显示和判断传感器慢变化。

`Mean_Accumulator` 使用六十四位有符号整数保存累加和，并用三十二位无符号整数记录样本数量。`Mean_addSample()` 加入单点，两个块接口加入连续数组。`Mean_get()` 在最后统一除以样本数量，并对正负结果进行对称四舍五入。累加器可以跨多个 DMA 块保留，避免保存所有历史样本；样本计数达到上限后会拒绝继续加入，防止计数和累加范围失效。

## 使用方式

只计算一个数组时，直接调用 `Mean_calculateInt16()` 或 `Mean_calculateInt32()`。需要合并多个 DMA 块时，先调用 `Mean_reset()`，再重复调用加入接口，最后使用 `Mean_get()`。开始下一次统计周期前再次复位。空数组没有平均值，计算接口会返回 `false`。

## 示例一：计算一路 ADC 块平均值

ADC 原始码不超过 4095，可以直接转为 `int16_t`。下例先抽取 `vin`，再计算当前 512 帧数据的平均原始码。

```c
#include "ti_msp_dl_config.h"
#include "Drivers/adc_multi.h"
#include "Algorithms/measurements/mean/mean.h"

static int16_t gVinCodes[ADC_MULTI_FRAME_COUNT];
static int32_t gVinMeanCode;

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

            if (Mean_calculateInt16(
                gVinCodes, frameCount, &gVinMeanCode)) {
                /* gVinMeanCode 是当前数据块的平均 ADC 原始码。 */
            }
        }
    }
}
```

原始 DMA 缓冲区释放后不能再读取 `frames`，因此示例先复制目标通道。若只需要平均值，也可以在持有 DMA 缓冲区时直接用累加器逐点加入，然后立即释放，不必建立中间数组。

## 示例二：直接统计校准后的物理量

下面把 `vin` 校准为外部输入毫伏，并对一个 DMA 块求平均。物理量可能超过有符号十六位，因此使用 `int32_t` 接口。

```c
static int32_t gVinMillivolts[ADC_MULTI_FRAME_COUNT];
static int32_t gMeanMillivolts;

for (index = 0U; index < frameCount; index++) {
    gVinMillivolts[index] = ADCCalibration_apply(
        &gVinCalibration, frames[index].vin);
}
ADCMulti_releaseBuffer(frames);

Mean_calculateInt32(
    gVinMillivolts, frameCount, &gMeanMillivolts);
```

## 示例三：四通道跨多个 DMA 块平均

每个通道使用一个累加器。下面连续统计十个数据块，得到更稳定的平均值，然后开始下一轮。

```c
#define MEAN_BLOCK_COUNT  (10U)

static Mean_Accumulator gMeanAcc[ADC_MULTI_CHANNEL_COUNT];
static int32_t gChannelMean[ADC_MULTI_CHANNEL_COUNT];
static uint8_t gAccumulatedBlocks;

static void ChannelMean_reset(void)
{
    uint8_t channel;

    for (channel = 0U; channel < ADC_MULTI_CHANNEL_COUNT; channel++) {
        Mean_reset(&gMeanAcc[channel]);
    }
    gAccumulatedBlocks = 0U;
}

static bool ChannelMean_addFrames(
    const ADCMulti_Frame *frames, uint16_t count)
{
    uint16_t i;

    for (i = 0U; i < count; i++) {
        if (!Mean_addSample(
                &gMeanAcc[ADC_MULTI_VIN], frames[i].vin) ||
            !Mean_addSample(
                &gMeanAcc[ADC_MULTI_IIN], frames[i].iin) ||
            !Mean_addSample(
                &gMeanAcc[ADC_MULTI_VOUT], frames[i].vout) ||
            !Mean_addSample(
                &gMeanAcc[ADC_MULTI_IOUT], frames[i].iout)) {
            return false;
        }
    }

    gAccumulatedBlocks++;
    if (gAccumulatedBlocks >= MEAN_BLOCK_COUNT) {
        uint8_t channel;

        for (channel = 0U;
             channel < ADC_MULTI_CHANNEL_COUNT; channel++) {
            Mean_get(&gMeanAcc[channel], &gChannelMean[channel]);
        }
        return true;
    }
    return false;
}
```

读取结果后应调用 `ChannelMean_reset()` 开始新的统计周期。若信号变化较快，累计太多数据块会使显示响应迟缓。平均值容易被尖峰拉动，存在明显异常点时可以先使用三点中值滤波，但不要为了稳定显示而掩盖真实故障。

## 注意事项

- 平均 ADC 原始码适合检查零偏，但换算物理量时应先校准。
- 有符号交流数据的平均值接近零，不代表信号幅度很小；幅度应查看 RMS 或峰峰值。
- 统计周期应与信号性质匹配，周期波形最好包含足够多的完整周期。
- `Mean_Accumulator` 使用前必须调用 `Mean_reset()`，不能依赖未初始化内存。
- 多通道不能共用正在写入的累加器，但可以使用相同的处理函数。
