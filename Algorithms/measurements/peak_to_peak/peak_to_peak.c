#include "Algorithms/measurements/peak_to_peak/peak_to_peak.h"

#include <limits.h>

static bool PeakToPeak_canAdd(
    const PeakToPeak_Accumulator *accumulator, uint32_t sampleCount)
{
    return (accumulator != 0) &&
           (sampleCount <= UINT32_MAX - accumulator->sampleCount);
}

void PeakToPeak_reset(PeakToPeak_Accumulator *accumulator)
{
    if (accumulator == 0) {
        return;
    }
    accumulator->minimum = INT32_MAX;
    accumulator->maximum = INT32_MIN;
    accumulator->sampleCount = 0U;
}

bool PeakToPeak_addSample(
    PeakToPeak_Accumulator *accumulator, int32_t sample)
{
    if (!PeakToPeak_canAdd(accumulator, 1U)) {
        return false;
    }
    if (sample < accumulator->minimum) {
        accumulator->minimum = sample;
    }
    if (sample > accumulator->maximum) {
        accumulator->maximum = sample;
    }
    accumulator->sampleCount++;
    return true;
}

bool PeakToPeak_addBlockInt16(PeakToPeak_Accumulator *accumulator,
    const int16_t *input, uint32_t sampleCount)
{
    uint32_t index;

    if (!PeakToPeak_canAdd(accumulator, sampleCount)) {
        return false;
    }
    if (sampleCount == 0U) {
        return true;
    }
    if (input == 0) {
        return false;
    }

    for (index = 0U; index < sampleCount; index++) {
        int32_t sample = input[index];

        if (sample < accumulator->minimum) {
            accumulator->minimum = sample;
        }
        if (sample > accumulator->maximum) {
            accumulator->maximum = sample;
        }
    }
    accumulator->sampleCount += sampleCount;
    return true;
}

bool PeakToPeak_addBlockInt32(PeakToPeak_Accumulator *accumulator,
    const int32_t *input, uint32_t sampleCount)
{
    uint32_t index;

    if (!PeakToPeak_canAdd(accumulator, sampleCount)) {
        return false;
    }
    if (sampleCount == 0U) {
        return true;
    }
    if (input == 0) {
        return false;
    }

    for (index = 0U; index < sampleCount; index++) {
        int32_t sample = input[index];

        if (sample < accumulator->minimum) {
            accumulator->minimum = sample;
        }
        if (sample > accumulator->maximum) {
            accumulator->maximum = sample;
        }
    }
    accumulator->sampleCount += sampleCount;
    return true;
}

bool PeakToPeak_get(const PeakToPeak_Accumulator *accumulator,
    PeakToPeak_Result *result)
{
    if ((accumulator == 0) || (result == 0) ||
        (accumulator->sampleCount == 0U)) {
        return false;
    }

    result->minimum = accumulator->minimum;
    result->maximum = accumulator->maximum;
    result->span = (uint32_t)
        ((int64_t) accumulator->maximum - accumulator->minimum);
    return true;
}

uint32_t PeakToPeak_getSampleCount(
    const PeakToPeak_Accumulator *accumulator)
{
    return (accumulator != 0) ? accumulator->sampleCount : 0U;
}

bool PeakToPeak_calculateInt16(const int16_t *input,
    uint32_t sampleCount, PeakToPeak_Result *result)
{
    PeakToPeak_Accumulator accumulator;

    PeakToPeak_reset(&accumulator);
    return PeakToPeak_addBlockInt16(&accumulator, input, sampleCount) &&
           PeakToPeak_get(&accumulator, result);
}

bool PeakToPeak_calculateInt32(const int32_t *input,
    uint32_t sampleCount, PeakToPeak_Result *result)
{
    PeakToPeak_Accumulator accumulator;

    PeakToPeak_reset(&accumulator);
    return PeakToPeak_addBlockInt32(&accumulator, input, sampleCount) &&
           PeakToPeak_get(&accumulator, result);
}
