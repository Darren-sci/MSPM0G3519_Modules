# Ripple 原理与使用方式

## 原理

电源输出通常由稳定直流和叠加其上的交流波动组成。Ripple 模块先计算整块数据的平均值作为直流分量，再用每个采样减去该平均值，剩余部分作为纹波。

模块同时给出：

- `dcMean`：输出直流平均值；
- `totalRms`：直流与纹波共同组成的总RMS；
- `rippleRms`：去除平均值后的交流纹波RMS；
- `ripplePeakToPeak`：最大值减最小值；
- 正、负峰值偏差：波形分别高于和低于平均值的最大距离；
- RMS纹波百分比和峰峰值纹波百分比。

纹波RMS反映纹波的总体能量，峰峰值对偶发尖峰和开关毛刺更敏感。两者描述的问题不同，电源测量最好同时报告。

## 百分比单位

纹波百分比以直流平均值的绝对值为基准：

```text
rippleRmsMilliPercent = 1000  表示1.000% RMS纹波
ripplePeakToPeakMilliPercent = 1000 表示1.000%峰峰值纹波
```

`rippleFactorQ15` 中32768表示纹波RMS等于直流值，即100%。如果直流平均值为零，百分比没有定义，此时 `percentageValid=false`，但纹波RMS和峰峰值仍然有效。

## 使用方式

```c
#include "Algorithms/power_analysis/ripple/ripple.h"

#define FRAME_COUNT  (512U)

static int32_t gOutputMilliVolt[FRAME_COUNT];
static Ripple_Result gOutputRipple;

static bool MeasureOutputRipple(void)
{
    if (!Ripple_calculate(
            gOutputMilliVolt,
            FRAME_COUNT,
            &gOutputRipple)) {
        return false;
    }

    return gOutputRipple.valid &&
           !gOutputRipple.calculationOverflow;
}
```

如果数据使用 mV，则下面这些结果也使用 mV：

```text
dcMean
totalRms
rippleRms
ripplePeakToPeak
positivePeakDeviation
negativePeakDeviation
```

例如采样值在4900 mV和5100 mV之间交替：

```text
dcMean = 5000 mV
rippleRms = 100 mV
ripplePeakToPeak = 200 mV
RMS纹波 = 2.000%
峰峰值纹波 = 4.000%
```

## 与FFT结合

Ripple 模块回答“纹波有多大”，但不回答“纹波主要是什么频率”。需要判断工频、二倍工频或开关频率时，可复制原始输出波形，去直流、加 Hann 窗后执行 FFT：

```text
原始校准波形
├→ Ripple：直流、RMS、峰峰值、百分比
└→ DC Removal → Hann → FFT：纹波频率和谐波
```

不要先经过 Hann 窗再计算纹波峰峰值或RMS，否则首尾被压低会改变真实结果。

## 采样率和测量窗口

测量窗口必须足够长，才能覆盖低频纹波。例如要测50 Hz或100 Hz整流纹波，几毫秒的数据明显不足。要观察高频开关纹波，采样率又必须足够高，并满足模拟前端带宽和抗混叠要求。

因此同一个采样配置不一定同时适合极低频漂移和几百kHz开关纹波。比赛现场应根据目标纹波频段选择采样率、FFT长度和模拟前端带宽。

## 注意事项

- 输入应是校准后的电压物理量，不要使用加窗后的FFT输入。
- 自动减去块平均值会把非常缓慢的负载变化部分视为纹波；测量定义必须明确时间窗口。
- 峰峰值容易受到单个ADC毛刺影响，需要时可同时报告中值滤波前后结果，但不能隐瞒真实尖峰。
- ADC噪声、参考电压噪声和前端运放噪声都会进入纹波RMS。
- 纹波幅度很小时，应使用合适量程或交流耦合放大，不能只依赖数字放大。
- 采样率不足时，高频纹波会混叠到错误低频；数字算法无法恢复已经混叠的信息。
- 模拟前端带宽过低会把真实纹波滤掉，带宽过高又可能引入大量开关尖峰和噪声，测量结果必须注明带宽。
- `totalRms` 不能当作 `rippleRms`；直流电源中两者数值可能相差几十倍。
- 平均值接近零时纹波百分比没有意义，应直接报告RMS和峰峰值。
