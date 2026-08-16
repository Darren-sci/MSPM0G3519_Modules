# Spectrum 原理与使用方式

## 原理

FFT 输出的每个频点都是一个复数，实部和虚部共同表示该频率的幅值与相位。Spectrum 模块把这种底层复数结果转换成可用于仪表显示和后续分析的单边幅值谱。

频点强度先由实部平方与虚部平方之和得到，再通过整数平方根得到复数模值。由于实数 ADC 信号的正、负频率对称，一个普通交流分量会被平均分到两个频点。因此单边幅值谱中，除直流点和奈奎斯特点外，其余频点需要乘2。

当前 FFT 已经在内部累计除以 N，所以 Spectrum 不再除以 FFT 长度。如果 FFT 前使用 Hann 或 Flat-top 窗，还要除以窗口的相干增益，才能恢复正弦幅值。未加窗时传入 `SPECTRUM_UNITY_GAIN_Q15`。

## 关键函数

- `Spectrum_init()`：绑定 FFT 长度、采样率和窗相干增益。
- `Spectrum_magnitudeSquaredQ30()`：计算实部平方与虚部平方之和，适合比较频点强弱，避免开方。
- `Spectrum_magnitudeQ15()`：使用整数平方根得到复数模值。
- `Spectrum_getAmplitudeQ15()`：完成复数模值、单边系数和窗增益补偿。
- `Spectrum_buildAmplitudeQ15()`：一次生成 `0～N/2` 的完整单边幅值数组。
- `Spectrum_binToFrequencyMilliHz()`：把频点换算为实际频率，使用 mHz 减少整数截断。

幅值输出使用 `uint32_t`，而不是强制限制到32767。这样窗增益配置错误或补偿后超过 ADC 满量程时，不会静默饱和，应用层可以识别异常。

## 使用方式

```c
#include "Algorithms/transforms/fft/fft.h"
#include "Algorithms/spectral_analysis/spectrum/spectrum.h"

#define FFT_LENGTH  (512U)
#define SAMPLE_RATE_HZ  (51200U)

static FFT_PlanQ15 gFFTPlan;
static FFT_ComplexQ15 gTwiddles[FFT_LENGTH / 2U];
static FFT_ComplexQ15 gFFTOutput[FFT_LENGTH];
static Spectrum_Config gSpectrumConfig;
static uint32_t gAmplitude[FFT_LENGTH / 2U + 1U];

static bool SpectrumModule_init(void)
{
    if (!FFT_init(&gFFTPlan, FFT_LENGTH,
            gTwiddles, FFT_LENGTH / 2U)) {
        return false;
    }

    return Spectrum_init(&gSpectrumConfig, &gFFTPlan,
        SAMPLE_RATE_HZ, SPECTRUM_UNITY_GAIN_Q15);
}

static bool BuildSpectrum(const int16_t *timeData)
{
    FFT_ExecutionInfo fftInfo;

    if (!FFT_executeReal(
            &gFFTPlan, timeData, gFFTOutput, &fftInfo) ||
        (fftInfo.saturationCount != 0U)) {
        return false;
    }

    return Spectrum_buildAmplitudeQ15(
        &gSpectrumConfig, gFFTOutput,
        gAmplitude, FFT_LENGTH / 2U + 1U);
}
```

使用 Hann 窗时，初始化频谱配置应传入实际相干增益：

```c
Spectrum_init(&gSpectrumConfig, &gFFTPlan,
    SAMPLE_RATE_HZ,
    Hann_getCoherentGainQ15(&gHannWindow));
```

Flat-top 同理使用 `FlatTop_getCoherentGainQ15()`。频谱配置必须与真正使用的窗保持一致；未加窗却传入 Hann 增益，会把幅值错误放大约两倍。

## 频点与频率

频点间隔等于采样率除以 FFT 长度。51200 Hz采样率配合512点 FFT 时，每个频点间隔100 Hz：

```c
uint64_t frequencyMilliHz =
    Spectrum_binToFrequencyMilliHz(&gSpectrumConfig, 10U);
/* 结果为1000000 mHz，即1000 Hz。 */
```

反向寻找最近频点：

```c
uint16_t bin = Spectrum_frequencyMilliHzToNearestBin(
    &gSpectrumConfig, 1000000U);
```

## 注意事项

- 输入必须是本项目同长度 `FFT_execute()` 的输出。
- ADC 原始无符号码必须先校准、转换为有符号 Q15，并按需要去直流和加窗。
- `Spectrum_buildAmplitudeQ15()` 输出数组至少包含 N/2+1 个 `uint32_t`。
- 直流点和奈奎斯特点不乘2，其他单边频点才乘2。
- 相干增益用于正弦幅值；噪声和功率分析不能用相干增益代替功率增益。
- 信号不在整数频点时能量会泄漏到附近频点，单个频点幅值可能低于真实值。
- Flat-top 更适合准确测量单个正弦幅值；Hann 更适合一般频谱、基波和谐波观察。
- 本模块输出仍是 Q15 码值尺度，不会自动换算为伏特、电流或 dBV。

