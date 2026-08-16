#include "Algorithms/spectral_analysis/thd/thd.h"

#include <limits.h>

static void THD_clearResult(THD_Result *result)
{
    result->harmonicPowerQ30 = 0U;
    result->fundamentalAmplitudeQ15 = 0U;
    result->harmonicRmsAmplitudeQ15 = 0U;
    result->thdQ15 = 0U;
    result->thdMilliPercent = 0U;
    result->includedHarmonicCount = 0U;
    result->powerSaturated = false;
    result->valid = false;
}

static uint32_t THD_integerSquareRoot(uint64_t value)
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
    if (value > result) {
        result++;
    }
    return (result > UINT32_MAX) ? UINT32_MAX : (uint32_t) result;
}

static uint32_t THD_ratioRoundedSaturated(
    uint32_t numerator, uint32_t denominator, uint32_t scale)
{
    uint64_t scaled = (uint64_t) numerator * scale;
    uint64_t result = (scaled + denominator / 2U) / denominator;

    return (result > UINT32_MAX) ? UINT32_MAX : (uint32_t) result;
}

bool THD_calculate(const HarmonicAnalysis_Component *components,
    uint16_t componentCount, THD_Result *result)
{
    uint16_t index;
    uint64_t harmonicPower = 0U;
    uint32_t harmonicRms;

    if (result == 0) {
        return false;
    }
    THD_clearResult(result);

    if ((components == 0) || (componentCount == 0U) ||
        !components[0].valid || (components[0].order != 1U) ||
        (components[0].amplitudeQ15 == 0U)) {
        return false;
    }

    for (index = 1U; index < componentCount; index++) {
        uint64_t square;

        if (!components[index].valid ||
            (components[index].order < 2U)) {
            continue;
        }

        square = (uint64_t) components[index].amplitudeQ15 *
            components[index].amplitudeQ15;
        if (UINT64_MAX - harmonicPower < square) {
            harmonicPower = UINT64_MAX;
            result->powerSaturated = true;
        } else {
            harmonicPower += square;
        }
        if (result->includedHarmonicCount != UINT16_MAX) {
            result->includedHarmonicCount++;
        }
    }

    harmonicRms = THD_integerSquareRoot(harmonicPower);
    result->harmonicPowerQ30 = harmonicPower;
    result->fundamentalAmplitudeQ15 = components[0].amplitudeQ15;
    result->harmonicRmsAmplitudeQ15 = harmonicRms;
    result->thdQ15 = THD_ratioRoundedSaturated(
        harmonicRms, components[0].amplitudeQ15, 32768U);
    result->thdMilliPercent = THD_ratioRoundedSaturated(
        harmonicRms, components[0].amplitudeQ15, 100000U);
    result->valid = true;
    return true;
}
