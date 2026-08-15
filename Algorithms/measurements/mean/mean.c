#include "Algorithms/measurements/mean/mean.h"

#include <limits.h>

static int32_t Mean_divideRounded(int64_t sum, uint32_t sampleCount)
{
    int64_t halfCount = (int64_t) sampleCount / 2;

    if (sum >= 0) {
        return (int32_t) ((sum + halfCount) / sampleCount);
    }
    return (int32_t) -((-sum + halfCount) / sampleCount);
}

static bool Mean_canAdd(
    const Mean_Accumulator *accumulator, uint32_t sampleCount)
{
    return (accumulator != 0) &&
           (sampleCount <= UINT32_MAX - accumulator->sampleCount);
}

void Mean_reset(Mean_Accumulator *accumulator)
{
    if (accumulator == 0) {
        return;
    }
    accumulator->sum = 0;
    accumulator->sampleCount = 0U;
}

bool Mean_addSample(Mean_Accumulator *accumulator, int32_t sample)
{
    if (!Mean_canAdd(accumulator, 1U)) {
        return false;
    }
    accumulator->sum += sample;
    accumulator->sampleCount++;
    return true;
}

bool Mean_addBlockInt16(Mean_Accumulator *accumulator,
    const int16_t *input, uint32_t sampleCount)
{
    uint32_t index;

    if (!Mean_canAdd(accumulator, sampleCount)) {
        return false;
    }
    if (sampleCount == 0U) {
        return true;
    }
    if (input == 0) {
        return false;
    }

    for (index = 0U; index < sampleCount; index++) {
        accumulator->sum += input[index];
    }
    accumulator->sampleCount += sampleCount;
    return true;
}

bool Mean_addBlockInt32(Mean_Accumulator *accumulator,
    const int32_t *input, uint32_t sampleCount)
{
    uint32_t index;

    if (!Mean_canAdd(accumulator, sampleCount)) {
        return false;
    }
    if (sampleCount == 0U) {
        return true;
    }
    if (input == 0) {
        return false;
    }

    for (index = 0U; index < sampleCount; index++) {
        accumulator->sum += input[index];
    }
    accumulator->sampleCount += sampleCount;
    return true;
}

bool Mean_get(const Mean_Accumulator *accumulator, int32_t *mean)
{
    if ((accumulator == 0) || (mean == 0) ||
        (accumulator->sampleCount == 0U)) {
        return false;
    }
    *mean = Mean_divideRounded(
        accumulator->sum, accumulator->sampleCount);
    return true;
}

uint32_t Mean_getSampleCount(const Mean_Accumulator *accumulator)
{
    return (accumulator != 0) ? accumulator->sampleCount : 0U;
}

bool Mean_calculateInt16(
    const int16_t *input, uint32_t sampleCount, int32_t *mean)
{
    Mean_Accumulator accumulator;

    Mean_reset(&accumulator);
    return Mean_addBlockInt16(&accumulator, input, sampleCount) &&
           Mean_get(&accumulator, mean);
}

bool Mean_calculateInt32(
    const int32_t *input, uint32_t sampleCount, int32_t *mean)
{
    Mean_Accumulator accumulator;

    Mean_reset(&accumulator);
    return Mean_addBlockInt32(&accumulator, input, sampleCount) &&
           Mean_get(&accumulator, mean);
}
