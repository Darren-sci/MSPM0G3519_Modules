#include "Algorithms/power_analysis/instantaneous_power/instantaneous_power.h"

#include <limits.h>
#include <stddef.h>

static bool InstantaneousPower_rangesOverlap(
    const void *first, size_t firstSize,
    const void *second, size_t secondSize)
{
    uintptr_t firstStart = (uintptr_t) first;
    uintptr_t secondStart = (uintptr_t) second;

    return (firstStart < secondStart + secondSize) &&
           (secondStart < firstStart + firstSize);
}

static bool InstantaneousPower_addI64(
    int64_t first, int64_t second, int64_t *result)
{
    if ((second > 0) && (first > INT64_MAX - second)) {
        *result = INT64_MAX;
        return false;
    }
    if ((second < 0) && (first < INT64_MIN - second)) {
        *result = INT64_MIN;
        return false;
    }
    *result = first + second;
    return true;
}

static int64_t InstantaneousPower_divideRounded(
    int64_t numerator, uint64_t denominator)
{
    uint64_t magnitude;
    uint64_t rounded;

    if (numerator >= 0) {
        magnitude = (uint64_t) numerator;
        rounded = magnitude / denominator;
        if ((magnitude % denominator) >=
            denominator / 2U + denominator % 2U) {
            rounded++;
        }
        return (int64_t) rounded;
    }

    magnitude = (uint64_t) (-(numerator + 1)) + 1U;
    rounded = magnitude / denominator;
    if ((magnitude % denominator) >=
        denominator / 2U + denominator % 2U) {
        rounded++;
    }
    if (rounded == (UINT64_C(1) << 63)) {
        return INT64_MIN;
    }
    return -(int64_t) rounded;
}

int64_t InstantaneousPower_calculateSample(
    int32_t voltage, int32_t current)
{
    return (int64_t) voltage * current;
}

bool InstantaneousPower_calculateBlock(
    const int32_t *voltage, const int32_t *current,
    int64_t *power, uint32_t sampleCount)
{
    uint32_t index;

    if (sampleCount == 0U) {
        return true;
    }
#if SIZE_MAX <= UINT32_MAX
    if ((sampleCount > (uint32_t) (SIZE_MAX / sizeof(*voltage))) ||
        (sampleCount > (uint32_t) (SIZE_MAX / sizeof(*power)))) {
        return false;
    }
#endif
    if ((voltage == 0) || (current == 0) || (power == 0) ||
        InstantaneousPower_rangesOverlap(voltage,
            (size_t) sampleCount * sizeof(*voltage),
            power, (size_t) sampleCount * sizeof(*power)) ||
        InstantaneousPower_rangesOverlap(current,
            (size_t) sampleCount * sizeof(*current),
            power, (size_t) sampleCount * sizeof(*power))) {
        return false;
    }

    for (index = 0U; index < sampleCount; index++) {
        power[index] = InstantaneousPower_calculateSample(
            voltage[index], current[index]);
    }
    return true;
}

bool InstantaneousPower_resetAccumulator(
    InstantaneousPower_Accumulator *accumulator)
{
    if (accumulator == 0) {
        return false;
    }
    accumulator->sumPower = 0;
    accumulator->sampleCount = 0U;
    accumulator->saturated = false;
    return true;
}

bool InstantaneousPower_accumulateSample(
    InstantaneousPower_Accumulator *accumulator,
    int32_t voltage, int32_t current)
{
    int64_t newSum;

    if ((accumulator == 0) || accumulator->saturated) {
        return false;
    }
    if (accumulator->sampleCount == UINT64_MAX) {
        accumulator->saturated = true;
        return false;
    }
    if (!InstantaneousPower_addI64(
            accumulator->sumPower,
            InstantaneousPower_calculateSample(voltage, current),
            &newSum)) {
        accumulator->sumPower = newSum;
        accumulator->saturated = true;
        return false;
    }

    accumulator->sumPower = newSum;
    accumulator->sampleCount++;
    return true;
}

bool InstantaneousPower_accumulateBlock(
    InstantaneousPower_Accumulator *accumulator,
    const int32_t *voltage, const int32_t *current,
    uint32_t sampleCount)
{
    uint32_t index;

    if (accumulator == 0) {
        return false;
    }
    if (sampleCount == 0U) {
        return !accumulator->saturated;
    }
    if ((voltage == 0) || (current == 0)) {
        return false;
    }

    for (index = 0U; index < sampleCount; index++) {
        if (!InstantaneousPower_accumulateSample(
                accumulator, voltage[index], current[index])) {
            return false;
        }
    }
    return true;
}

bool InstantaneousPower_getAverage(
    const InstantaneousPower_Accumulator *accumulator,
    int64_t *averagePower)
{
    if ((accumulator == 0) || (averagePower == 0) ||
        accumulator->saturated || (accumulator->sampleCount == 0U)) {
        return false;
    }

    *averagePower = InstantaneousPower_divideRounded(
        accumulator->sumPower, accumulator->sampleCount);
    return true;
}

bool InstantaneousPower_getEnergy(
    const InstantaneousPower_Accumulator *accumulator,
    uint32_t sampleRateHz, int64_t *energy)
{
    if ((accumulator == 0) || (energy == 0) ||
        accumulator->saturated || (accumulator->sampleCount == 0U) ||
        (sampleRateHz == 0U)) {
        return false;
    }

    *energy = InstantaneousPower_divideRounded(
        accumulator->sumPower, sampleRateHz);
    return true;
}
