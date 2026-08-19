#ifndef ALGORITHMS_ANALYSIS_PIPELINE_SIGNAL_ANALYZER_H_
#define ALGORITHMS_ANALYSIS_PIPELINE_SIGNAL_ANALYZER_H_

#include <stdbool.h>
#include <stdint.h>

#include "Algorithms/measurements/duty_cycle/duty_cycle.h"
#include "Algorithms/measurements/frequency/frequency.h"
#include "Algorithms/measurements/peak_to_peak/peak_to_peak.h"
#include "Algorithms/preprocessing/adc_calibration/adc_calibration.h"
#include "Algorithms/preprocessing/window_functions/flat_top/flat_top.h"
#include "Algorithms/preprocessing/window_functions/hann/hann.h"
#include "Algorithms/spectral_analysis/fundamental_detection/fundamental_detection.h"
#include "Algorithms/spectral_analysis/harmonic_analysis/harmonic_analysis.h"
#include "Algorithms/spectral_analysis/spectrum/spectrum.h"
#include "Algorithms/spectral_analysis/thd/thd.h"
#include "Algorithms/transforms/fft/fft.h"

/* 与现有 FFT 模块保持一致，避免统一接口暗中申请动态内存。 */
#define SIGNAL_ANALYZER_MAX_LENGTH       FFT_MAX_LENGTH
#define SIGNAL_ANALYZER_MAX_HARMONICS    (6U)
#define SIGNAL_ANALYZER_MAX_REAL_BINS    \
    ((SIGNAL_ANALYZER_MAX_LENGTH / 2U) + 1U)
#define SIGNAL_ANALYZER_MAX_TWIDDLES     \
    (SIGNAL_ANALYZER_MAX_LENGTH / 2U)

/** 可按位组合的分析功能。昂贵的 FFT 仅在启用频谱类功能时运行。 */
typedef enum {
    SIGNAL_ANALYZER_FEATURE_BASIC       = (1UL << 0),
    SIGNAL_ANALYZER_FEATURE_FREQUENCY   = (1UL << 1),
    SIGNAL_ANALYZER_FEATURE_DUTY        = (1UL << 2),
    SIGNAL_ANALYZER_FEATURE_SPECTRUM    = (1UL << 3),
    SIGNAL_ANALYZER_FEATURE_FUNDAMENTAL = (1UL << 4),
    SIGNAL_ANALYZER_FEATURE_HARMONICS   = (1UL << 5),
    SIGNAL_ANALYZER_FEATURE_THD         = (1UL << 6)
} SignalAnalyzer_Feature;

/** 频谱分析所用窗口。基础时域参数始终在加窗前计算。 */
typedef enum {
    SIGNAL_ANALYZER_WINDOW_NONE = 0,
    SIGNAL_ANALYZER_WINDOW_HANN,
    SIGNAL_ANALYZER_WINDOW_FLAT_TOP
} SignalAnalyzer_WindowType;

/**
 * 调用状态。RESULT_READY 表示刚好完成一帧分析；NEED_MORE_DATA 表示内部
 * 缓冲区尚未积满，两者都属于正常状态。
 */
typedef enum {
    SIGNAL_ANALYZER_STATUS_OK = 0,
    SIGNAL_ANALYZER_STATUS_RESULT_READY,
    SIGNAL_ANALYZER_STATUS_NEED_MORE_DATA,
    SIGNAL_ANALYZER_STATUS_INVALID_ARGUMENT,
    SIGNAL_ANALYZER_STATUS_NOT_INITIALIZED,
    SIGNAL_ANALYZER_STATUS_CALIBRATION_REQUIRED,
    SIGNAL_ANALYZER_STATUS_CONFIGURATION_ERROR,
    SIGNAL_ANALYZER_STATUS_PROCESSING_ERROR
} SignalAnalyzer_Status;

/** 单通道统一分析配置。频率搜索上下限均使用 mHz。 */
typedef struct {
    uint32_t sampleRateHz;
    uint16_t analysisLength;
    uint32_t enabledFeatures;
    SignalAnalyzer_WindowType windowType;

    int16_t edgeThresholdQ15;
    uint16_t edgeHysteresisQ15;

    uint64_t minimumFrequencyMilliHz;
    uint64_t maximumFrequencyMilliHz;
    uint16_t maximumHarmonicOrder;
    uint16_t harmonicSearchRadiusBins;

    bool rawCalibrationEnabled;
    ADCCalibration_Config rawCalibration;
} SignalAnalyzer_Config;

/**
 * 统一接口使用的静态工作区。
 *
 * 应定义为 static 或全局对象，不要作为普通局部变量放到较小的任务栈中。
 * samplesQ15 在完成时域测量后会被原地加窗；下一帧会覆盖旧数据。
 */
typedef struct {
    int16_t samplesQ15[SIGNAL_ANALYZER_MAX_LENGTH];
    int16_t windowCoefficients[SIGNAL_ANALYZER_MAX_LENGTH];
    FFT_ComplexQ15 fftData[SIGNAL_ANALYZER_MAX_LENGTH];
    FFT_ComplexQ15 twiddles[SIGNAL_ANALYZER_MAX_TWIDDLES];
    uint32_t amplitudeSpectrumQ15[SIGNAL_ANALYZER_MAX_REAL_BINS];
} SignalAnalyzer_Workspace;

/** 一帧分析结果；validFeatures 指示哪些成员本次有效。 */
typedef struct {
    uint32_t sequence;
    uint32_t sampleRateHz;
    uint16_t sampleCount;
    uint32_t validFeatures;

    int32_t meanQ15;
    PeakToPeak_Result peakToPeak;
    uint32_t rmsQ15;
    uint32_t acRmsQ15;

    Frequency_Result frequency;
    DutyCycle_Result duty;

    FFT_ExecutionInfo fftInfo;
    FundamentalDetection_Result fundamental;
    HarmonicAnalysis_Component
        harmonics[SIGNAL_ANALYZER_MAX_HARMONICS];
    uint16_t harmonicCount;
    THD_Result thd;
} SignalAnalyzer_Result;

/** 运行状态。初始化后除非重新配置，否则调用者不应直接修改成员。 */
typedef struct {
    SignalAnalyzer_Config config;
    SignalAnalyzer_Workspace *workspace;
    FFT_PlanQ15 fftPlan;
    Spectrum_Config spectrumConfig;
    Hann_Window hannWindow;
    FlatTop_Window flatTopWindow;
    uint16_t collectedSamples;
    uint32_t sequence;
    bool spectralEnabled;
    bool initialized;
    bool spectrumAvailable;
} SignalAnalyzer;

/**
 * 生成一份适合一般周期信号的默认配置。
 * 默认启用基础参数、频率和占空比，阈值为0，尚不启用原始ADC校准。
 */
bool SignalAnalyzer_getDefaultConfig(SignalAnalyzer_Config *config,
    uint32_t sampleRateHz, uint16_t analysisLength);

/** 初始化分析器、FFT旋转因子和窗口；成功返回OK，不会开始ADC采样。 */
SignalAnalyzer_Status SignalAnalyzer_init(SignalAnalyzer *analyzer,
    const SignalAnalyzer_Config *config,
    SignalAnalyzer_Workspace *workspace);

/** 丢弃尚未积满的一帧和上一次频谱，但保留全部配置。 */
void SignalAnalyzer_reset(SignalAnalyzer *analyzer);

/**
 * 加入带步长的 Q15 数据。
 *
 * strideElements=1 表示连续数组；四通道交错数据可使用4。函数最多消费到
 * 当前分析帧完成为止，实际消费数量通过 consumedSamples 返回。若输入仍
 * 有剩余，调用者可移动指针后再次调用，从而不会静默丢失采样。
 */
SignalAnalyzer_Status SignalAnalyzer_pushQ15(
    SignalAnalyzer *analyzer,
    const int16_t *input, uint32_t sampleCount,
    uint16_t strideElements, uint32_t *consumedSamples,
    SignalAnalyzer_Result *result);

/**
 * 加入带步长的原始 ADC 码。必须先在配置中启用并设置 rawCalibration；
 * 校准结果会饱和为Q15。strideElements 的单位是 uint16_t 元素。
 */
SignalAnalyzer_Status SignalAnalyzer_pushRawADC(
    SignalAnalyzer *analyzer,
    const uint16_t *input, uint32_t sampleCount,
    uint16_t strideElements, uint32_t *consumedSamples,
    SignalAnalyzer_Result *result);

/** 返回当前已经累计但尚未分析的采样数量。 */
uint16_t SignalAnalyzer_getCollectedSampleCount(
    const SignalAnalyzer *analyzer);

/**
 * 取得最近一帧的单边幅值谱。指针指向工作区，下一帧分析后内容会更新。
 * 未启用完整幅度谱或尚未产生结果时返回 false。
 */
bool SignalAnalyzer_getAmplitudeSpectrum(
    const SignalAnalyzer *analyzer,
    const uint32_t **amplitudeQ15, uint16_t *binCount);

#endif
