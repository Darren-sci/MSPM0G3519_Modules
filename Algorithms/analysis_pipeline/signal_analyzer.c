#include "Algorithms/analysis_pipeline/signal_analyzer.h"

#include <limits.h>
#include <string.h>

#include "Algorithms/measurements/mean/mean.h"
#include "Algorithms/measurements/rms/rms.h"
#include "Algorithms/preprocessing/dc_removal/dc_removal.h"

#define SIGNAL_ANALYZER_SPECTRAL_FEATURES \
    (SIGNAL_ANALYZER_FEATURE_SPECTRUM | \
     SIGNAL_ANALYZER_FEATURE_FUNDAMENTAL | \
     SIGNAL_ANALYZER_FEATURE_HARMONICS | \
     SIGNAL_ANALYZER_FEATURE_THD)

#define SIGNAL_ANALYZER_ALL_FEATURES \
    (SIGNAL_ANALYZER_FEATURE_BASIC | \
     SIGNAL_ANALYZER_FEATURE_FREQUENCY | \
     SIGNAL_ANALYZER_FEATURE_DUTY | \
     SIGNAL_ANALYZER_SPECTRAL_FEATURES)

static bool SignalAnalyzer_calibrationIsValid(
    const ADCCalibration_Config *calibration)
{
    return (calibration != 0) &&
           (calibration->lowCode <= calibration->zeroCode) &&
           (calibration->zeroCode < calibration->highCode) &&
           (calibration->lowValue >= INT16_MIN) &&
           (calibration->lowValue <= 0) &&
           (calibration->highValue > 0) &&
           (calibration->highValue <= INT16_MAX);
}

static bool SignalAnalyzer_configIsValid(
    const SignalAnalyzer_Config *config)
{
    uint64_t nyquistMilliHz;

    if ((config == 0) || (config->sampleRateHz == 0U) ||
        !FFT_isLengthSupported(config->analysisLength) ||
        ((config->enabledFeatures & ~SIGNAL_ANALYZER_ALL_FEATURES) != 0U) ||
        (config->enabledFeatures == 0U) ||
        (config->windowType > SIGNAL_ANALYZER_WINDOW_FLAT_TOP) ||
        (config->maximumHarmonicOrder > SIGNAL_ANALYZER_MAX_HARMONICS)) {
        return false;
    }

    if ((config->enabledFeatures & SIGNAL_ANALYZER_SPECTRAL_FEATURES) != 0U) {
        nyquistMilliHz = (uint64_t) config->sampleRateHz * 500U;
        if ((config->minimumFrequencyMilliHz == 0U) ||
            (config->minimumFrequencyMilliHz >
                config->maximumFrequencyMilliHz) ||
            (config->maximumFrequencyMilliHz > nyquistMilliHz)) {
            return false;
        }
    }

    if (((config->enabledFeatures &
             (SIGNAL_ANALYZER_FEATURE_HARMONICS |
              SIGNAL_ANALYZER_FEATURE_THD)) != 0U) &&
        (config->maximumHarmonicOrder == 0U)) {
        return false;
    }

    return !config->rawCalibrationEnabled ||
           SignalAnalyzer_calibrationIsValid(&config->rawCalibration);
}

static void SignalAnalyzer_clearResult(SignalAnalyzer_Result *result)
{
    if (result != 0) {
        (void) memset(result, 0, sizeof(*result));
    }
}

static bool SignalAnalyzer_prepareSpectralPath(
    SignalAnalyzer *analyzer)
{
    SignalAnalyzer_Workspace *workspace = analyzer->workspace;
    uint32_t coherentGainQ15 = SPECTRUM_UNITY_GAIN_Q15;

    if (!FFT_init(&analyzer->fftPlan, analyzer->config.analysisLength,
            workspace->twiddles, SIGNAL_ANALYZER_MAX_TWIDDLES)) {
        return false;
    }

    if (analyzer->config.windowType == SIGNAL_ANALYZER_WINDOW_HANN) {
        if (!Hann_init(&analyzer->hannWindow,
                workspace->windowCoefficients,
                analyzer->config.analysisLength)) {
            return false;
        }
        coherentGainQ15 = Hann_getCoherentGainQ15(&analyzer->hannWindow);
    } else if (analyzer->config.windowType ==
               SIGNAL_ANALYZER_WINDOW_FLAT_TOP) {
        if (!FlatTop_init(&analyzer->flatTopWindow,
                workspace->windowCoefficients,
                analyzer->config.analysisLength)) {
            return false;
        }
        coherentGainQ15 =
            FlatTop_getCoherentGainQ15(&analyzer->flatTopWindow);
    }

    return Spectrum_init(&analyzer->spectrumConfig,
        &analyzer->fftPlan, analyzer->config.sampleRateHz,
        coherentGainQ15);
}

bool SignalAnalyzer_getDefaultConfig(SignalAnalyzer_Config *config,
    uint32_t sampleRateHz, uint16_t analysisLength)
{
    uint64_t firstBinMilliHz;

    if ((config == 0) || (sampleRateHz == 0U) ||
        !FFT_isLengthSupported(analysisLength)) {
        return false;
    }

    (void) memset(config, 0, sizeof(*config));
    firstBinMilliHz =
        ((uint64_t) sampleRateHz * 1000U + analysisLength / 2U) /
        analysisLength;

    config->sampleRateHz = sampleRateHz;
    config->analysisLength = analysisLength;
    config->enabledFeatures = SIGNAL_ANALYZER_FEATURE_BASIC |
        SIGNAL_ANALYZER_FEATURE_FREQUENCY |
        SIGNAL_ANALYZER_FEATURE_DUTY;
    config->windowType = SIGNAL_ANALYZER_WINDOW_HANN;
    config->edgeThresholdQ15 = 0;
    config->edgeHysteresisQ15 = 512U;
    config->minimumFrequencyMilliHz = firstBinMilliHz;
    config->maximumFrequencyMilliHz = (uint64_t) sampleRateHz * 500U;
    config->maximumHarmonicOrder = SIGNAL_ANALYZER_MAX_HARMONICS;
    config->harmonicSearchRadiusBins = 1U;
    config->rawCalibrationEnabled = false;
    return true;
}

SignalAnalyzer_Status SignalAnalyzer_init(SignalAnalyzer *analyzer,
    const SignalAnalyzer_Config *config,
    SignalAnalyzer_Workspace *workspace)
{
    if ((analyzer == 0) || (config == 0) || (workspace == 0)) {
        return SIGNAL_ANALYZER_STATUS_INVALID_ARGUMENT;
    }
    if (!SignalAnalyzer_configIsValid(config)) {
        return SIGNAL_ANALYZER_STATUS_CONFIGURATION_ERROR;
    }

    (void) memset(analyzer, 0, sizeof(*analyzer));
    analyzer->config = *config;
    analyzer->workspace = workspace;
    analyzer->spectralEnabled =
        (config->enabledFeatures & SIGNAL_ANALYZER_SPECTRAL_FEATURES) != 0U;

    if (analyzer->spectralEnabled &&
        !SignalAnalyzer_prepareSpectralPath(analyzer)) {
        (void) memset(analyzer, 0, sizeof(*analyzer));
        return SIGNAL_ANALYZER_STATUS_CONFIGURATION_ERROR;
    }

    analyzer->initialized = true;
    return SIGNAL_ANALYZER_STATUS_OK;
}

void SignalAnalyzer_reset(SignalAnalyzer *analyzer)
{
    if ((analyzer == 0) || !analyzer->initialized) {
        return;
    }
    analyzer->collectedSamples = 0U;
    analyzer->spectrumAvailable = false;
}

static bool SignalAnalyzer_analyzeTimeDomain(
    SignalAnalyzer *analyzer, SignalAnalyzer_Result *result)
{
    const int16_t *samples = analyzer->workspace->samplesQ15;
    uint32_t count = analyzer->config.analysisLength;
    uint32_t features = analyzer->config.enabledFeatures;

    if ((features & SIGNAL_ANALYZER_FEATURE_BASIC) != 0U) {
        if (!Mean_calculateInt16(samples, count, &result->meanQ15) ||
            !PeakToPeak_calculateInt16(
                samples, count, &result->peakToPeak) ||
            !RMS_calculate(samples, count, &result->rmsQ15) ||
            !RMS_calculateAC(
                samples, count, &result->acRmsQ15, 0)) {
            return false;
        }
        result->validFeatures |= SIGNAL_ANALYZER_FEATURE_BASIC;
    }

    if ((features & SIGNAL_ANALYZER_FEATURE_FREQUENCY) != 0U) {
        if (Frequency_calculate(samples, count,
                analyzer->config.sampleRateHz,
                analyzer->config.edgeThresholdQ15,
                analyzer->config.edgeHysteresisQ15,
                &result->frequency)) {
            result->validFeatures |= SIGNAL_ANALYZER_FEATURE_FREQUENCY;
        }
    }

    if ((features & SIGNAL_ANALYZER_FEATURE_DUTY) != 0U) {
        if (DutyCycle_calculate(samples, count,
                analyzer->config.edgeThresholdQ15,
                analyzer->config.edgeHysteresisQ15,
                &result->duty)) {
            result->validFeatures |= SIGNAL_ANALYZER_FEATURE_DUTY;
        }
    }
    return true;
}

static bool SignalAnalyzer_applyWindow(SignalAnalyzer *analyzer)
{
    int16_t *samples = analyzer->workspace->samplesQ15;

    if (analyzer->config.windowType == SIGNAL_ANALYZER_WINDOW_HANN) {
        return Hann_apply(&analyzer->hannWindow, samples, samples);
    }
    if (analyzer->config.windowType == SIGNAL_ANALYZER_WINDOW_FLAT_TOP) {
        return FlatTop_apply(&analyzer->flatTopWindow, samples, samples);
    }
    return true;
}

static bool SignalAnalyzer_getSearchBins(const SignalAnalyzer *analyzer,
    uint16_t *firstBin, uint16_t *lastBin)
{
    uint16_t binCount = Spectrum_getBinCount(&analyzer->spectrumConfig);

    if ((firstBin == 0) || (lastBin == 0) || (binCount < 2U)) {
        return false;
    }

    *firstBin = Spectrum_frequencyMilliHzToNearestBin(
        &analyzer->spectrumConfig,
        analyzer->config.minimumFrequencyMilliHz);
    *lastBin = Spectrum_frequencyMilliHzToNearestBin(
        &analyzer->spectrumConfig,
        analyzer->config.maximumFrequencyMilliHz);

    if (*firstBin == 0U) {
        *firstBin = 1U;
    }
    if (*lastBin >= binCount) {
        *lastBin = (uint16_t) (binCount - 1U);
    }
    return *firstBin <= *lastBin;
}

static bool SignalAnalyzer_analyzeSpectrum(
    SignalAnalyzer *analyzer, SignalAnalyzer_Result *result)
{
    SignalAnalyzer_Workspace *workspace = analyzer->workspace;
    uint32_t features = analyzer->config.enabledFeatures;
    uint16_t firstBin;
    uint16_t lastBin;
    bool needFundamental =
        (features & (SIGNAL_ANALYZER_FEATURE_FUNDAMENTAL |
                     SIGNAL_ANALYZER_FEATURE_HARMONICS |
                     SIGNAL_ANALYZER_FEATURE_THD)) != 0U;
    bool needHarmonics =
        (features & (SIGNAL_ANALYZER_FEATURE_HARMONICS |
                     SIGNAL_ANALYZER_FEATURE_THD)) != 0U;

    /*
     * 时域指标已经使用原始校准数据计算完毕。FFT前在同一工作缓冲区中
     * 去除本帧均值，避免直流偏置经过窗函数后泄漏到第1个频点并被误判
     * 为基波；随后再执行加窗和FFT。
     */
    if (!DCRemoval_processInt16(workspace->samplesQ15,
            workspace->samplesQ15,
            analyzer->config.analysisLength, 0) ||
        !SignalAnalyzer_applyWindow(analyzer) ||
        !FFT_executeReal(&analyzer->fftPlan, workspace->samplesQ15,
            workspace->fftData, &result->fftInfo)) {
        return false;
    }

    if ((features & SIGNAL_ANALYZER_FEATURE_SPECTRUM) != 0U) {
        if (!Spectrum_buildAmplitudeQ15(&analyzer->spectrumConfig,
                workspace->fftData, workspace->amplitudeSpectrumQ15,
                SIGNAL_ANALYZER_MAX_REAL_BINS)) {
            return false;
        }
        analyzer->spectrumAvailable = true;
        result->validFeatures |= SIGNAL_ANALYZER_FEATURE_SPECTRUM;
    }

    if (!needFundamental) {
        return true;
    }
    if (!SignalAnalyzer_getSearchBins(analyzer, &firstBin, &lastBin) ||
        !FundamentalDetection_find(&analyzer->spectrumConfig,
            workspace->fftData, firstBin, lastBin,
            &result->fundamental)) {
        return true;
    }
    if ((features & SIGNAL_ANALYZER_FEATURE_FUNDAMENTAL) != 0U) {
        result->validFeatures |= SIGNAL_ANALYZER_FEATURE_FUNDAMENTAL;
    }

    if (!needHarmonics ||
        !HarmonicAnalysis_analyze(&analyzer->spectrumConfig,
            workspace->fftData, result->fundamental.frequencyMilliHz,
            analyzer->config.maximumHarmonicOrder,
            analyzer->config.harmonicSearchRadiusBins,
            result->harmonics, SIGNAL_ANALYZER_MAX_HARMONICS)) {
        return true;
    }
    result->harmonicCount = analyzer->config.maximumHarmonicOrder;
    if ((features & SIGNAL_ANALYZER_FEATURE_HARMONICS) != 0U) {
        result->validFeatures |= SIGNAL_ANALYZER_FEATURE_HARMONICS;
    }

    if (((features & SIGNAL_ANALYZER_FEATURE_THD) != 0U) &&
        THD_calculate(result->harmonics,
            result->harmonicCount, &result->thd)) {
        result->validFeatures |= SIGNAL_ANALYZER_FEATURE_THD;
    }
    return true;
}

static SignalAnalyzer_Status SignalAnalyzer_finishFrame(
    SignalAnalyzer *analyzer, SignalAnalyzer_Result *result)
{
    SignalAnalyzer_clearResult(result);
    analyzer->spectrumAvailable = false;

    result->sequence = ++analyzer->sequence;
    result->sampleRateHz = analyzer->config.sampleRateHz;
    result->sampleCount = analyzer->config.analysisLength;

    if (!SignalAnalyzer_analyzeTimeDomain(analyzer, result) ||
        (analyzer->spectralEnabled &&
         !SignalAnalyzer_analyzeSpectrum(analyzer, result))) {
        analyzer->collectedSamples = 0U;
        return SIGNAL_ANALYZER_STATUS_PROCESSING_ERROR;
    }

    analyzer->collectedSamples = 0U;
    return SIGNAL_ANALYZER_STATUS_RESULT_READY;
}

static SignalAnalyzer_Status SignalAnalyzer_validatePush(
    SignalAnalyzer *analyzer, const void *input,
    uint32_t sampleCount, uint16_t strideElements,
    uint32_t *consumedSamples, SignalAnalyzer_Result *result)
{
    if (consumedSamples != 0) {
        *consumedSamples = 0U;
    }
    if ((analyzer == 0) || !analyzer->initialized) {
        return SIGNAL_ANALYZER_STATUS_NOT_INITIALIZED;
    }
    if ((consumedSamples == 0) || (result == 0) ||
        (strideElements == 0U) ||
        ((sampleCount != 0U) && (input == 0))) {
        return SIGNAL_ANALYZER_STATUS_INVALID_ARGUMENT;
    }
    return SIGNAL_ANALYZER_STATUS_NEED_MORE_DATA;
}

SignalAnalyzer_Status SignalAnalyzer_pushQ15(
    SignalAnalyzer *analyzer,
    const int16_t *input, uint32_t sampleCount,
    uint16_t strideElements, uint32_t *consumedSamples,
    SignalAnalyzer_Result *result)
{
    SignalAnalyzer_Status status = SignalAnalyzer_validatePush(
        analyzer, input, sampleCount, strideElements,
        consumedSamples, result);
    uint32_t available;
    uint32_t consume;
    uint32_t index;

    if (status != SIGNAL_ANALYZER_STATUS_NEED_MORE_DATA) {
        return status;
    }

    available = analyzer->config.analysisLength -
        analyzer->collectedSamples;
    consume = (sampleCount < available) ? sampleCount : available;
    for (index = 0U; index < consume; index++) {
        analyzer->workspace->samplesQ15[
            analyzer->collectedSamples + index] =
            input[index * (uint32_t) strideElements];
    }
    analyzer->collectedSamples =
        (uint16_t) (analyzer->collectedSamples + consume);
    *consumedSamples = consume;

    if (analyzer->collectedSamples == analyzer->config.analysisLength) {
        return SignalAnalyzer_finishFrame(analyzer, result);
    }
    return SIGNAL_ANALYZER_STATUS_NEED_MORE_DATA;
}

SignalAnalyzer_Status SignalAnalyzer_pushRawADC(
    SignalAnalyzer *analyzer,
    const uint16_t *input, uint32_t sampleCount,
    uint16_t strideElements, uint32_t *consumedSamples,
    SignalAnalyzer_Result *result)
{
    SignalAnalyzer_Status status = SignalAnalyzer_validatePush(
        analyzer, input, sampleCount, strideElements,
        consumedSamples, result);
    uint32_t available;
    uint32_t consume;
    uint32_t index;

    if (status != SIGNAL_ANALYZER_STATUS_NEED_MORE_DATA) {
        return status;
    }
    if (!analyzer->config.rawCalibrationEnabled) {
        return SIGNAL_ANALYZER_STATUS_CALIBRATION_REQUIRED;
    }

    available = analyzer->config.analysisLength -
        analyzer->collectedSamples;
    consume = (sampleCount < available) ? sampleCount : available;
    for (index = 0U; index < consume; index++) {
        analyzer->workspace->samplesQ15[
            analyzer->collectedSamples + index] =
            ADCCalibration_applyToInt16(
                &analyzer->config.rawCalibration,
                input[index * (uint32_t) strideElements]);
    }
    analyzer->collectedSamples =
        (uint16_t) (analyzer->collectedSamples + consume);
    *consumedSamples = consume;

    if (analyzer->collectedSamples == analyzer->config.analysisLength) {
        return SignalAnalyzer_finishFrame(analyzer, result);
    }
    return SIGNAL_ANALYZER_STATUS_NEED_MORE_DATA;
}

uint16_t SignalAnalyzer_getCollectedSampleCount(
    const SignalAnalyzer *analyzer)
{
    return ((analyzer != 0) && analyzer->initialized) ?
        analyzer->collectedSamples : 0U;
}

bool SignalAnalyzer_getAmplitudeSpectrum(
    const SignalAnalyzer *analyzer,
    const uint32_t **amplitudeQ15, uint16_t *binCount)
{
    if ((analyzer == 0) || !analyzer->initialized ||
        !analyzer->spectrumAvailable ||
        (amplitudeQ15 == 0) || (binCount == 0)) {
        return false;
    }

    *amplitudeQ15 = analyzer->workspace->amplitudeSpectrumQ15;
    *binCount = Spectrum_getBinCount(&analyzer->spectrumConfig);
    return *binCount != 0U;
}
