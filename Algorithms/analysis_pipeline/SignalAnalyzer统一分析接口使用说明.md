# SignalAnalyzer 单通道统一分析接口

## 1. 作用

`SignalAnalyzer` 把项目中已有的校准、时域测量、FFT、基波、谐波和 THD
模块组合成一个单通道入口。它适合接收 ADC DMA 数据块，但不依赖 ADC0、
ADC1 或具体驱动，因此同一套接口可以用于四通道 ADC 中的任意一路，也可以
用于以后增加的 ADC1 高速单通道。

处理顺序如下：

```text
原始ADC码或Q15数据
        ↓
可选ADC校准
        ↓
基础参数、频率、占空比（原始时域数据）
        ↓
可选Hann/Flat-top窗
        ↓
FFT、幅度谱、基波、谐波、THD
```

基础测量在加窗前完成，因此窗口不会改变 RMS、峰峰值、频率和占空比。

## 2. 主要特点

- 不使用动态内存和浮点运算。
- 支持连续数组和交错多通道数组。
- 可以跨多个 DMA 缓冲区累计一帧，例如两个 512 点块组成一次 1024 点FFT。
- 使用功能位选择算法，没有启用频谱功能时不会运行FFT。
- 单项测量失败只会清除对应有效位，不影响同一帧的其他有效结果。
- 输入有剩余时通过 `consumedSamples` 告知调用者，不会静默丢弃数据。
- 初始化成功返回 `SIGNAL_ANALYZER_STATUS_OK`，不会自动启动任何ADC或定时器。

## 3. 内存说明

`SignalAnalyzer_Workspace` 按最大 1024 点配置，包含采样、窗系数、FFT、
旋转因子和幅度谱缓冲区。对象较大，应定义为全局变量或 `static` 变量：

```c
static SignalAnalyzer gAnalyzer;
static SignalAnalyzer_Workspace gAnalyzerWorkspace;
static SignalAnalyzer_Result gAnalyzerResult;
```

不要把工作区定义为普通局部变量，否则可能占满主栈。

## 4. 最小Q15示例

```c
#include "Algorithms/analysis_pipeline/signal_analyzer.h"

static SignalAnalyzer gAnalyzer;
static SignalAnalyzer_Workspace gWorkspace;
static SignalAnalyzer_Result gResult;

void Analysis_init(void)
{
    SignalAnalyzer_Config config;

    SignalAnalyzer_getDefaultConfig(&config, 100000U, 1024U);
    config.enabledFeatures |=
        SIGNAL_ANALYZER_FEATURE_SPECTRUM |
        SIGNAL_ANALYZER_FEATURE_FUNDAMENTAL |
        SIGNAL_ANALYZER_FEATURE_HARMONICS |
        SIGNAL_ANALYZER_FEATURE_THD;
    config.minimumFrequencyMilliHz = 100U * 1000U;
    config.maximumFrequencyMilliHz = 10000U * 1000U;

    (void) SignalAnalyzer_init(&gAnalyzer, &config, &gWorkspace);
}

void Analysis_processQ15(const int16_t *samples, uint32_t count)
{
    while (count != 0U) {
        uint32_t consumed;
        SignalAnalyzer_Status status = SignalAnalyzer_pushQ15(
            &gAnalyzer, samples, count, 1U,
            &consumed, &gResult);

        samples += consumed;
        count -= consumed;

        if (status == SIGNAL_ANALYZER_STATUS_RESULT_READY) {
            /* 根据 validFeatures 决定可以显示哪些结果。 */
            if ((gResult.validFeatures &
                    SIGNAL_ANALYZER_FEATURE_FREQUENCY) != 0U) {
                /* gResult.frequency.frequencyMilliHz */
            }
        } else if (status != SIGNAL_ANALYZER_STATUS_NEED_MORE_DATA) {
            /* 参数、配置或处理错误。 */
            break;
        }
    }
}
```

## 5. 接入四通道ADC中的一路

`ADCMulti_Frame` 的成员顺序是 `vin/iin/vout/iout`，内存中每个通道相隔
4 个 `uint16_t`。下面以 `vout` 为例：

```c
static SignalAnalyzer gAnalyzer;
static SignalAnalyzer_Workspace gWorkspace;
static SignalAnalyzer_Result gResult;

void Analysis_initRawADC(void)
{
    SignalAnalyzer_Config config;

    SignalAnalyzer_getDefaultConfig(&config, 100000U, 1024U);

    /* 示例假定ADC中点约为2048，实际项目应换成实测三点标定码。 */
    config.rawCalibrationEnabled =
        ADCCalibration_initBipolarQ15(
            &config.rawCalibration, 0U, 2048U, 4095U);

    config.enabledFeatures |=
        SIGNAL_ANALYZER_FEATURE_FUNDAMENTAL |
        SIGNAL_ANALYZER_FEATURE_HARMONICS |
        SIGNAL_ANALYZER_FEATURE_THD;
    config.minimumFrequencyMilliHz = 100U * 1000U;
    config.maximumFrequencyMilliHz = 10000U * 1000U;

    (void) SignalAnalyzer_init(&gAnalyzer, &config, &gWorkspace);
}

void Analysis_processVout(
    const ADCMulti_Frame *frames, uint16_t frameCount)
{
    const uint16_t *input = &frames[0].vout;
    const uint16_t stride =
        (uint16_t) (sizeof(ADCMulti_Frame) / sizeof(uint16_t));
    uint32_t remaining = frameCount;

    while (remaining != 0U) {
        uint32_t consumed;
        SignalAnalyzer_Status status = SignalAnalyzer_pushRawADC(
            &gAnalyzer, input, remaining, stride,
            &consumed, &gResult);

        input += consumed * stride;
        remaining -= consumed;

        if ((status != SIGNAL_ANALYZER_STATUS_RESULT_READY) &&
            (status != SIGNAL_ANALYZER_STATUS_NEED_MORE_DATA)) {
            break;
        }
    }
}
```

注意：示例中的 `100000U` 必须与 ADC 实际每通道采样率一致，应优先使用
`ADCMulti_getActualFrameRate()` 得到的值初始化分析器。

## 6. 功能依赖

- `BASIC`：均值、最大值、最小值、峰峰值、总RMS和交流RMS。
- `FREQUENCY`：基于带滞回上升沿测频，需要数据中至少出现两个有效上升沿。
- `DUTY`：基于带滞回边沿测量，占空比信号应有明确高低电平。
- `SPECTRUM`：生成完整单边幅度谱，可通过
  `SignalAnalyzer_getAmplitudeSpectrum()` 取得。
- `FUNDAMENTAL`：在配置的频率范围内寻找最强基波。
- `HARMONICS`：会在基波整数倍附近搜索，内部自动执行基波检测。
- `THD`：依赖内部谐波结果，计算THD但不包含噪声。

启用 `HARMONICS` 或 `THD` 时不强制启用完整 `SPECTRUM`，这样只关心几个
频率分量时可以省去生成整幅幅度谱的时间。

## 7. 阈值与窗口选择

双极性 Q15 信号通常可从以下参数开始调试：

```c
config.edgeThresholdQ15 = 0;
config.edgeHysteresisQ15 = 512U;
```

噪声较大时增大滞回，幅度很小时减小滞回。频率和占空比始终使用未加窗
数据。

- Hann窗：综合性能较好，适合频率、谐波和THD分析。
- Flat-top窗：幅值准确性更好，但主瓣更宽，邻近频率分辨能力较弱。
- 不加窗：仅适合同步整周期采样，否则频谱泄漏明显。

## 8. 有效位和状态

函数返回 `RESULT_READY` 只表示一帧处理完毕，不表示每个指标都有效。
例如一帧不足一个完整周期时，基础RMS仍然有效，但频率有效位可能为0。
显示前必须检查：

```c
if ((result.validFeatures & SIGNAL_ANALYZER_FEATURE_THD) != 0U) {
    /* result.thd.thdMilliPercent 的单位为0.001%。 */
}
```

FFT执行信息中的 `saturationCount` 非0时，说明输入余量不足，频谱可能失真。

## 9. 当前边界

- 分析长度仅支持16～1024的2次幂。
- 当前只分析单通道，不计算通道间相位和功率。
- 尚未包含波形类型识别、单次触发和预触发。
- 原始ADC入口输出统一为Q15；如需直接显示mV、mA，应另外保留工程量校准结果。
