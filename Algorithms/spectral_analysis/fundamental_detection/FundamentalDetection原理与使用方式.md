# Fundamental Detection 原理与使用方式

## 原理

基波通常是周期信号中最低、最主要的频率分量。Fundamental Detection 在调用者指定的频率范围内比较各频点的实部平方与虚部平方之和，找到功率最大的频点，再根据左右相邻频点进行抛物线插值，估计峰值在两个离散频点之间的偏移。

FFT 只能直接给出离散频点。插值不能增加真正的频率分辨率，但能在信号稳定、信噪比较好并且使用合适窗函数时，减小“只能返回整频点”的量化误差。返回的 `binOffsetQ15` 约在 -0.5～+0.5 个频点之间。

最大谱线不一定总是基波：直流偏置、开关尖峰或高次谐波可能更强。因此模块要求应用层提供搜索范围。一般应从1号频点开始以排除直流，并根据题目可能的信号频率限制最高搜索频点。

## 使用方式

```c
#include "Algorithms/spectral_analysis/fundamental_detection/fundamental_detection.h"

static FundamentalDetection_Result gFundamental;

static bool FindFundamental(void)
{
    uint16_t firstBin =
        Spectrum_frequencyMilliHzToNearestBin(
            &gSpectrumConfig, 100000U);       /* 100 Hz */
    uint16_t lastBin =
        Spectrum_frequencyMilliHzToNearestBin(
            &gSpectrumConfig, 20000000U);     /* 20 kHz */

    return FundamentalDetection_find(
        &gSpectrumConfig,
        gFFTOutput,
        firstBin,
        lastBin,
        &gFundamental);
}
```

成功后可以读取：

```c
uint16_t integerPeakBin = gFundamental.peakBin;
int16_t fractionalOffset = gFundamental.binOffsetQ15;
uint64_t frequencyMilliHz = gFundamental.frequencyMilliHz;
uint32_t amplitudeQ15 = gFundamental.amplitudeQ15;
```

如果 `atSearchBoundary` 为 true，峰值正好落在搜索区间边缘。此时模块不会进行相邻点插值，通常还意味着搜索范围可能过窄，应谨慎相信该结果。

## 注意事项

- 应先完成 FFT，不能把时域数据传给本模块。
- 推荐排除0号直流点；去直流不能完全替代合理的搜索范围。
- 全部候选频点都为零时函数返回 false。
- 插值得到的是估计值，不代表 FFT 分辨率真的提高。
- 多音信号中模块只返回搜索范围内最强分量，最强分量不一定符合物理意义上的基波。
- Flat-top 主瓣较宽，不一定适合精细频率插值；一般频率检测优先 Hann。
- `amplitudeQ15` 使用峰值整数频点计算，没有根据插值位置重新拟合幅值。

