# Phase 原理与使用方式

## 原理

相位模块分别寻找参考信号和目标信号的确认上升沿，先用参考信号的相邻上升沿计算平均周期，再把每个参考边沿与时间上最近的目标边沿配对。两者时间差会折返到正负半个周期内，最后换算成千分之一度。

边沿检测与频率模块相同：先记录中心阈值的线性插值位置，再使用滞回边界确认，边沿时刻采用 Q16.16 采样点。`delayQ16` 为正表示目标边沿发生得更晚；按照常见相位约定，目标更晚代表滞后，所以 `phaseMilliDegrees` 为负。输出限制在负 180 度到正 180 度范围内。

## 使用方式

准备两路等长且时间对应的数据，以及各自的阈值和滞回参数，再调用 `Phase_calculate()`。两路必须同频，参考数据至少包含两个确认上升沿。相位模块一次处理完整数据块，不保存跨块状态；低频信号应收集更长的连续数组后再测量。

## 示例一：测量 VIN 与 VOUT 相位

```c
#include "ti_msp_dl_config.h"
#include "Drivers/adc_multi.h"
#include "Algorithms/measurements/phase/phase.h"

static int16_t gVin[ADC_MULTI_FRAME_COUNT];
static int16_t gVout[ADC_MULTI_FRAME_COUNT];
static Phase_Result gVoltagePhase;

int main(void)
{
    const ADCMulti_Frame *frames;
    uint16_t frameCount;
    uint16_t i;
    Phase_Config config = {
        .referenceThreshold = 2048,
        .targetThreshold = 2048,
        .referenceHysteresis = 50U,
        .targetHysteresis = 50U,
        .targetTimeOffsetQ16 = 0
    };

    SYSCFG_DL_init();
    ADCMulti_init();
    if (!ADCMulti_start(ADC_MULTI_DEFAULT_RATE_HZ)) {
        while (1) {
        }
    }

    while (1) {
        if (ADCMulti_getReadyBuffer(&frames, &frameCount)) {
            for (i = 0U; i < frameCount; i++) {
                gVin[i] = (int16_t) frames[i].vin;
                gVout[i] = (int16_t) frames[i].vout;
            }
            ADCMulti_releaseBuffer(frames);

            if (Phase_calculate(gVin, gVout,
                frameCount, &config, &gVoltagePhase)) {
                int32_t phaseMilliDegrees =
                    gVoltagePhase.phaseMilliDegrees;
                /* 正数表示 VOUT 超前 VIN，负数表示 VOUT 滞后。 */
            }
        }
    }
}
```

两路允许使用不同阈值，因为各通道零偏和幅度可能不同。更推荐先分别校准或去除直流，再统一使用零阈值。若两路先经过滤波，必须保证滤波器结构、阶数和延迟一致。

## ADC 顺序采样补偿

当前 ADC0 按 `vin、iin、vout、iout` 顺序转换，并不是真正同时采样。目标通道相对参考通道的实际采样时刻差，应换算成一个帧周期的 Q16.16 比例，填入 `targetTimeOffsetQ16`。目标采样更晚时填写正值。

如果暂时不知道硬件转换间隔，可以把同一个稳定正弦信号同时接入两路，在偏移配置为零时测量 `delayQ16`。理想真实相位差为零，因此把测得延迟的相反数作为待验证的偏移补偿，再用多组频率检查。模拟前端自身的运放、RC 网络和通道差异也会产生相移，单靠 ADC 时序补偿不能消除。

## 示例二：已知目标通道晚四分之一采样点

```c
Phase_Config config = {
    .referenceThreshold = 0,
    .targetThreshold = 0,
    .referenceHysteresis = 512U,
    .targetHysteresis = 512U,
    .targetTimeOffsetQ16 = 16384
};

Phase_calculate(gVinQ15, gVoutQ15,
    frameCount, &config, &gVoltagePhase);
```

Q16.16 中一个完整采样点是 65536，因此 16384 表示四分之一采样点。这个值必须来自时序计算或同相信号标定，不能仅根据通道序号猜测。

## 示例三：相位和时间延迟的解释

```c
int64_t delayMicroseconds;
uint32_t sampleRate = ADCMulti_getActualFrameRate();

delayMicroseconds =
    (gVoltagePhase.delayQ16 * 1000000LL) /
    ((int64_t) sampleRate * 65536LL);
```

`delayMicroseconds` 为正表示目标通道边沿较晚。相位是周期内的相对位置，同一时间延迟在不同频率下对应不同相位角。

## 注意事项

- 两路信号必须同频；频率不同或缓慢漂移时，单一相位结果没有稳定意义。
- 波形严重失真或一个周期多次穿越阈值时，上升沿配对可能错误。
- 接近正负 180 度时，微小噪声可能使显示在两端跳变，需要做角度展开或圆周平均。
- 数据块应包含多个完整周期，只有两个边沿时结果对抖动很敏感。
- 四通道 ADC 是顺序采样，高频相位测量必须考虑通道时间差。
- 电压与电流相位用于功率因数时，还必须校准传感器和模拟前端的固有相移。
