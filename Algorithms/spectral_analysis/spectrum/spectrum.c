#include "Algorithms/spectral_analysis/spectrum/spectrum.h"

#include <stddef.h>

static bool Spectrum_rangesOverlap(const void *first, size_t firstSize,
    const void *second, size_t secondSize)
{
    uintptr_t firstStart = (uintptr_t) first;
    uintptr_t secondStart = (uintptr_t) second;

    return (firstStart < secondStart + secondSize) &&
           (secondStart < firstStart + firstSize);
}

static bool Spectrum_isConfigValid(const Spectrum_Config *config)
{
    return (config != 0) && config->initialized &&
           FFT_isLengthSupported(config->fftLength) &&
           (config->realBinCount == config->fftLength / 2U + 1U) &&
           (config->sampleRateHz != 0U) &&
           (config->coherentGainQ15 != 0U) &&
           (config->coherentGainQ15 <= SPECTRUM_UNITY_GAIN_Q15);
}

static uint32_t Spectrum_integerSquareRoot(uint64_t value)
{
    uint64_t result = 0U;
    uint64_t bit = UINT64_C(1) << 62;

    while (bit > value) {
        bit >>= 2;
    }

    while (bit != 0U) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }

    /* value 是余数；大于 result 时，四舍五入到下一个整数根。 */
    if (value > result) {
        result++;
    }
    return (uint32_t) result;
}

bool Spectrum_init(Spectrum_Config *config,
    const FFT_PlanQ15 *fftPlan, uint32_t sampleRateHz,
    uint32_t coherentGainQ15)
{
    uint16_t fftLength;

    if (config == 0) {
        return false;
    }

    config->sampleRateHz = 0U;
    config->coherentGainQ15 = 0U;
    config->fftLength = 0U;
    config->realBinCount = 0U;
    config->initialized = false;

    fftLength = FFT_getLength(fftPlan);
    if ((fftLength == 0U) || (sampleRateHz == 0U) ||
        (coherentGainQ15 == 0U) ||
        (coherentGainQ15 > SPECTRUM_UNITY_GAIN_Q15)) {
        return false;
    }

    config->sampleRateHz = sampleRateHz;
    config->coherentGainQ15 = coherentGainQ15;
    config->fftLength = fftLength;
    config->realBinCount = (uint16_t) (fftLength / 2U + 1U);
    config->initialized = true;
    return true;
}

uint16_t Spectrum_getBinCount(const Spectrum_Config *config)
{
    return Spectrum_isConfigValid(config) ? config->realBinCount : 0U;
}

uint64_t Spectrum_binToFrequencyMilliHz(
    const Spectrum_Config *config, uint16_t bin)
{
    uint64_t numerator;

    if (!Spectrum_isConfigValid(config) ||
        (bin >= config->realBinCount)) {
        return 0U;
    }

    numerator = (uint64_t) bin * config->sampleRateHz * 1000U;
    return (numerator + config->fftLength / 2U) / config->fftLength;
}

uint16_t Spectrum_frequencyMilliHzToNearestBin(
    const Spectrum_Config *config, uint64_t frequencyMilliHz)
{
    uint64_t numerator;
    uint64_t denominator;
    uint64_t bin;

    if (!Spectrum_isConfigValid(config)) {
        return 0U;
    }

    denominator = (uint64_t) config->sampleRateHz * 1000U;
    if (frequencyMilliHz >= denominator / 2U) {
        return (uint16_t) (config->realBinCount - 1U);
    }
    numerator = frequencyMilliHz * config->fftLength;
    bin = (numerator + denominator / 2U) / denominator;

    if (bin >= config->realBinCount) {
        return (uint16_t) (config->realBinCount - 1U);
    }
    return (uint16_t) bin;
}

uint32_t Spectrum_magnitudeSquaredQ30(FFT_ComplexQ15 value)
{
    int32_t real = value.real;
    int32_t imag = value.imag;

    return (uint32_t) ((int64_t) real * real + (int64_t) imag * imag);
}

uint32_t Spectrum_magnitudeQ15(FFT_ComplexQ15 value)
{
    return Spectrum_integerSquareRoot(
        Spectrum_magnitudeSquaredQ30(value));
}

bool Spectrum_getAmplitudeQ15(const Spectrum_Config *config,
    const FFT_ComplexQ15 *spectrum, uint16_t bin,
    uint32_t *amplitudeQ15)
{
    uint64_t numerator;
    uint32_t magnitude;

    if (!Spectrum_isConfigValid(config) || (spectrum == 0) ||
        (amplitudeQ15 == 0) || (bin >= config->realBinCount)) {
        return false;
    }

    magnitude = Spectrum_magnitudeQ15(spectrum[bin]);
    numerator = (uint64_t) magnitude * SPECTRUM_UNITY_GAIN_Q15;

    if ((bin != 0U) && (bin != config->fftLength / 2U)) {
        numerator *= 2U;
    }

    *amplitudeQ15 = (uint32_t)
        ((numerator + config->coherentGainQ15 / 2U) /
         config->coherentGainQ15);
    return true;
}

bool Spectrum_buildAmplitudeQ15(const Spectrum_Config *config,
    const FFT_ComplexQ15 *spectrum,
    uint32_t *output, uint16_t outputCapacity)
{
    uint16_t bin;

    if (!Spectrum_isConfigValid(config) || (spectrum == 0) ||
        (output == 0) || (outputCapacity < config->realBinCount) ||
        Spectrum_rangesOverlap(spectrum,
            (size_t) config->fftLength * sizeof(*spectrum),
            output,
            (size_t) config->realBinCount * sizeof(*output))) {
        return false;
    }

    for (bin = 0U; bin < config->realBinCount; bin++) {
        if (!Spectrum_getAmplitudeQ15(
                config, spectrum, bin, &output[bin])) {
            return false;
        }
    }
    return true;
}
