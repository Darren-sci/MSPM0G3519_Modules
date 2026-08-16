#include "Algorithms/spectral_analysis/harmonic_analysis/harmonic_analysis.h"

#include <limits.h>

static void HarmonicAnalysis_clearComponent(
    HarmonicAnalysis_Component *component, uint16_t order,
    uint64_t expectedFrequencyMilliHz)
{
    component->expectedFrequencyMilliHz = expectedFrequencyMilliHz;
    component->detectedFrequencyMilliHz = 0U;
    component->amplitudeQ15 = 0U;
    component->peakPowerQ30 = 0U;
    component->order = order;
    component->detectedBin = 0U;
    component->valid = false;
}

uint16_t HarmonicAnalysis_getMaximumOrder(
    const Spectrum_Config *config,
    uint64_t fundamentalFrequencyMilliHz)
{
    uint64_t nyquistMilliHz;
    uint64_t order;

    if ((Spectrum_getBinCount(config) == 0U) ||
        (fundamentalFrequencyMilliHz == 0U)) {
        return 0U;
    }

    nyquistMilliHz = (uint64_t) config->sampleRateHz * 500U;
    order = nyquistMilliHz / fundamentalFrequencyMilliHz;
    return (order > UINT16_MAX) ? UINT16_MAX : (uint16_t) order;
}

bool HarmonicAnalysis_analyze(const Spectrum_Config *config,
    const FFT_ComplexQ15 *spectrum,
    uint64_t fundamentalFrequencyMilliHz,
    uint16_t maximumOrder, uint16_t searchRadiusBins,
    HarmonicAnalysis_Component *components,
    uint16_t componentCapacity)
{
    uint16_t binCount = Spectrum_getBinCount(config);
    uint64_t nyquistMilliHz;
    uint32_t orderIndex;

    if ((binCount == 0U) || (spectrum == 0) ||
        (fundamentalFrequencyMilliHz == 0U) ||
        (maximumOrder == 0U) || (components == 0) ||
        (componentCapacity < maximumOrder)) {
        return false;
    }

    nyquistMilliHz = (uint64_t) config->sampleRateHz * 500U;

    for (orderIndex = 1U; orderIndex <= maximumOrder; orderIndex++) {
        uint16_t order = (uint16_t) orderIndex;
        HarmonicAnalysis_Component *component = &components[orderIndex - 1U];
        uint64_t expectedFrequency;
        uint16_t centerBin;
        uint16_t firstBin;
        uint16_t lastBin;
        uint16_t bin;
        uint16_t peakBin;
        uint32_t peakPower;

        if (fundamentalFrequencyMilliHz > UINT64_MAX / order) {
            expectedFrequency = UINT64_MAX;
        } else {
            expectedFrequency = fundamentalFrequencyMilliHz * order;
        }
        HarmonicAnalysis_clearComponent(
            component, order, expectedFrequency);

        if (expectedFrequency > nyquistMilliHz) {
            continue;
        }

        centerBin = Spectrum_frequencyMilliHzToNearestBin(
            config, expectedFrequency);
        firstBin = (centerBin > searchRadiusBins) ?
            (uint16_t) (centerBin - searchRadiusBins) : 1U;
        if (firstBin == 0U) {
            firstBin = 1U;
        }

        if ((uint32_t) centerBin + searchRadiusBins >= binCount) {
            lastBin = (uint16_t) (binCount - 1U);
        } else {
            lastBin = (uint16_t) (centerBin + searchRadiusBins);
        }
        if (firstBin > lastBin) {
            continue;
        }

        peakBin = firstBin;
        peakPower = Spectrum_magnitudeSquaredQ30(spectrum[firstBin]);
        for (bin = (uint16_t) (firstBin + 1U);
             bin <= lastBin; bin++) {
            uint32_t power =
                Spectrum_magnitudeSquaredQ30(spectrum[bin]);

            if (power > peakPower) {
                peakPower = power;
                peakBin = bin;
            }
        }

        if (peakPower == 0U) {
            continue;
        }

        component->detectedBin = peakBin;
        component->detectedFrequencyMilliHz =
            Spectrum_binToFrequencyMilliHz(config, peakBin);
        component->peakPowerQ30 = peakPower;
        if (!Spectrum_getAmplitudeQ15(
                config, spectrum, peakBin, &component->amplitudeQ15)) {
            return false;
        }
        component->valid = true;
    }
    return true;
}
