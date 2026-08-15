# Duty Cycle 原理与使用方式

## 原理

占空比模块检测同一阈值上的上升沿和下降沿。相邻上升沿之间是一个周期，上升沿到其后的下降沿是高电平时间。模块累计多个完整周期的高电平时间和总周期，再输出千分比占空比。结果 500 表示百分之五十，1000 表示百分之百。

`DutyCycle_processSample()` 先记录信号穿越中心阈值的插值位置，再等待信号达到滞回边界确认边沿。确认仍使用原中心阈值的位置，所以滞回用于抗噪，而不会故意把高电平时间扩宽或缩短。边沿位置采用 Q16.16 采样点，允许得到亚采样间隔的估计。只有“上升、下降、下一次上升”完整出现后，才累计一个有效周期。

## 使用方式

调用 `DutyCycle_init()` 设置阈值和滞回，随后连续加入单点或 DMA 数据块。`DutyCycle_getResult()` 返回千分比占空比、平均高电平时间、平均周期和有效周期数量。流式状态跨 DMA 块保存；切换通道、阈值或独立测量时应重新初始化或复位。

## 示例一：直接测量 ADC 方波占空比

假设输入是单极性方波，低电平码约为 300，高电平码约为 3700，因此阈值取 2000，滞回取 100 个 ADC 码。

```c
#include "ti_msp_dl_config.h"
#include "Drivers/adc_multi.h"
#include "Algorithms/measurements/duty_cycle/duty_cycle.h"

static DutyCycle_Meter gVinDuty;
static DutyCycle_Result gVinDutyResult;

int main(void)
{
    const ADCMulti_Frame *frames;
    uint16_t frameCount;
    uint16_t i;

    SYSCFG_DL_init();
    ADCMulti_init();
    DutyCycle_init(&gVinDuty, 2000, 100U);

    if (!ADCMulti_start(ADC_MULTI_DEFAULT_RATE_HZ)) {
        while (1) {
        }
    }

    while (1) {
        if (ADCMulti_getReadyBuffer(&frames, &frameCount)) {
            for (i = 0U; i < frameCount; i++) {
                DutyCycle_processSample(
                    &gVinDuty, (int16_t) frames[i].vin);
            }
            ADCMulti_releaseBuffer(frames);

            if (DutyCycle_getResult(&gVinDuty, &gVinDutyResult)) {
                uint16_t percentInteger =
                    gVinDutyResult.dutyPermille / 10U;
                uint16_t percentDecimal =
                    gVinDutyResult.dutyPermille % 10U;
                /* 显示 percentInteger.percentDecimal %。 */
            }
        }
    }
}
```

## 示例二：单个 Q15 数据块

```c
DutyCycle_Result result;

if (DutyCycle_calculate(
    gVinQ15, frameCount, 0, 512U, &result)) {
    /* result.dutyPermille 为 0～1000。 */
}
```

单块中必须包含完整的上升、下降和下一上升序列。低频信号周期长于数据块时，应使用流式接口连续处理多个 DMA 块。

## 示例三：由 Q16.16 时间换算微秒

```c
uint64_t highTimeUs;
uint64_t periodUs;
uint32_t sampleRate = ADCMulti_getActualFrameRate();

highTimeUs =
    (gVinDutyResult.averageHighTimeQ16 * 1000000U) /
    ((uint64_t) sampleRate * 65536U);
periodUs =
    (gVinDutyResult.averagePeriodQ16 * 1000000U) /
    ((uint64_t) sampleRate * 65536U);
```

乘法在长时间累计或极低频信号下可能接近六十四位上限，正式通用封装应先约分；当前 512 帧和常用比赛采样率范围内足够安全。

## 注意事项

- 阈值应位于稳定高低电平之间，不能直接使用不确定的零点。
- 滞回必须超过噪声幅度，但不能大到信号无法触及确认边界。
- 上升沿和下降沿过慢时，噪声会造成插值位置抖动。
- 占空比接近零或百分之百时，窄脉冲可能因采样率不足而完全漏掉。
- 中值或低通滤波可能改变窄脉冲宽度，测占空比前应验证滤波影响。
- 输入一直为高或一直为低时没有完整周期，接口会返回 `false`。
