# Harmonic Analysis 原理与使用方式

## 原理

如果基波频率为 `f0`，二次谐波理论上位于 `2×f0`，三次谐波位于 `3×f0`，依次类推。Harmonic Analysis 根据已经检测到的基波频率预测每次谐波位置，再在理论频点左右的小范围内寻找局部最大谱线。

局部搜索可以容忍基波频率估计误差、FFT频点取整和轻微采样时钟偏差。`searchRadiusBins` 越大，越不容易漏掉峰值，但也越可能把附近无关信号或另一条强谱线误认成谐波。

高于采样率一半的谐波无法由当前采样数据正确观察。模块会把这些分量标记为无效，不会把混叠后的频率当作原谐波。

## 使用方式

基波检测成功后，分析前10次分量：

```c
#include "Algorithms/spectral_analysis/harmonic_analysis/harmonic_analysis.h"

#define HARMONIC_COUNT  (10U)

static HarmonicAnalysis_Component gHarmonics[HARMONIC_COUNT];

static bool AnalyzeHarmonics(void)
{
    if (!gFundamental.valid) {
        return false;
    }

    return HarmonicAnalysis_analyze(
        &gSpectrumConfig,
        gFFTOutput,
        gFundamental.frequencyMilliHz,
        HARMONIC_COUNT,
        1U,                     /* 理论频点左右各搜索1格 */
        gHarmonics,
        HARMONIC_COUNT);
}
```

数组下标0对应基波，数组下标1对应二次谐波：

```c
if (gHarmonics[1].valid) {
    uint32_t secondAmplitude = gHarmonics[1].amplitudeQ15;
    uint16_t secondBin = gHarmonics[1].detectedBin;
}
```

可以预先判断理论上最多能看到多少次谐波：

```c
uint16_t maximumObservableOrder =
    HarmonicAnalysis_getMaximumOrder(
        &gSpectrumConfig,
        gFundamental.frequencyMilliHz);
```

## 窗和幅值

每次谐波幅值都通过 Spectrum 模块计算，所以已经包含单边谱系数和窗相干增益补偿。相同窗口的相干增益在谐波与基波幅值比中通常会抵消，但频率不在整数频点时仍会出现栅栏效应。

准确测量少量谐波幅值可考虑 Flat-top；需要分开距离较近的谱线或观察较多谐波时通常选择 Hann。当前实现取搜索范围内的最大单个频点，没有把整个主瓣能量积分起来。

## 注意事项

- 必须先获得可靠的基波频率；基波错误会使所有谐波搜索位置一起错误。
- `components` 容量至少等于 `maximumOrder`。
- `searchRadiusBins` 通常从0或1开始，不应盲目设置很大。
- 搜索窗口互相重叠时，同一条谱线可能被多个谐波次数重复选中。
- `valid=false` 可能表示该谐波高于奈奎斯特频率，或搜索范围内没有非零谱线。
- 谐波次数越高，信号通常越弱，更容易受到噪声底和频谱泄漏影响。
- 本模块只提取谐波，不判断其是否由被测电路产生，也不自动扣除信号源自身失真。

