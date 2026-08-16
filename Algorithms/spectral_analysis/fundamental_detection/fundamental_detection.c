#include "Algorithms/spectral_analysis/fundamental_detection/fundamental_detection.h"

static void FundamentalDetection_clearResult(
    FundamentalDetection_Result *result)
{
    result->frequencyMilliHz = 0U;
    result->amplitudeQ15 = 0U;
    result->peakPowerQ30 = 0U;
    result->peakBin = 0U;
    result->binOffsetQ15 = 0;
    result->atSearchBoundary = false;
    result->valid = false;
}

bool FundamentalDetection_find(const Spectrum_Config *config,
    const FFT_ComplexQ15 *spectrum,
    uint16_t firstBin, uint16_t lastBin,
    FundamentalDetection_Result *result)
{
    uint16_t binCount;
    uint16_t bin;
    uint16_t peakBin;
    uint32_t peakPower;
    int32_t offsetQ15 = 0;
    int64_t binPositionQ15;
    uint64_t frequencyNumerator;
    uint64_t frequencyDenominator;

    if (result == 0) {
        return false;
    }
    FundamentalDetection_clearResult(result);

    binCount = Spectrum_getBinCount(config);
    if ((binCount == 0U) || (spectrum == 0) ||
        (firstBin > lastBin) || (lastBin >= binCount)) {
        return false;
    }

    peakBin = firstBin;
    peakPower = Spectrum_magnitudeSquaredQ30(spectrum[firstBin]);
    for (bin = (uint16_t) (firstBin + 1U); bin <= lastBin; bin++) {
        uint32_t power = Spectrum_magnitudeSquaredQ30(spectrum[bin]);

        if (power > peakPower) {
            peakPower = power;
            peakBin = bin;
        }
    }

    if (peakPower == 0U) {
        return false;
    }

    result->atSearchBoundary =
        (peakBin == firstBin) || (peakBin == lastBin);

    if (!result->atSearchBoundary && (peakBin > 0U) &&
        ((uint16_t) (peakBin + 1U) < binCount)) {
        int64_t left = Spectrum_magnitudeSquaredQ30(
            spectrum[peakBin - 1U]);
        int64_t center = peakPower;
        int64_t right = Spectrum_magnitudeSquaredQ30(
            spectrum[peakBin + 1U]);
        int64_t denominator = left - 2 * center + right;

        if (denominator != 0) {
            int64_t calculated =
                ((left - right) * 16384) / denominator;

            if (calculated > 16384) {
                calculated = 16384;
            } else if (calculated < -16384) {
                calculated = -16384;
            }
            offsetQ15 = (int32_t) calculated;
        }
    }

    binPositionQ15 = (int64_t) peakBin * 32768 + offsetQ15;
    frequencyNumerator = (uint64_t) binPositionQ15 *
        config->sampleRateHz * 1000U;
    frequencyDenominator = (uint64_t) config->fftLength * 32768U;

    result->peakBin = peakBin;
    result->binOffsetQ15 = (int16_t) offsetQ15;
    result->peakPowerQ30 = peakPower;
    result->frequencyMilliHz =
        (frequencyNumerator + frequencyDenominator / 2U) /
        frequencyDenominator;
    if (!Spectrum_getAmplitudeQ15(
            config, spectrum, peakBin, &result->amplitudeQ15)) {
        FundamentalDetection_clearResult(result);
        return false;
    }
    result->valid = true;
    return true;
}
