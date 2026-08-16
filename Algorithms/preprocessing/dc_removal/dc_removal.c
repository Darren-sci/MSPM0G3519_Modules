#include "Algorithms/preprocessing/dc_removal/dc_removal.h"

#include <limits.h>

#include "Algorithms/measurements/mean/mean.h"

static int16_t DCRemoval_saturateInt16(int64_t value)
{
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t) value;
}

static int32_t DCRemoval_saturateInt32(int64_t value)
{
    if (value > INT32_MAX) {
        return INT32_MAX;
    }
    if (value < INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t) value;
}

bool DCRemoval_subtractInt16(const int16_t *input, int16_t *output,
    uint32_t sampleCount, int32_t dcValue)
{
    uint32_t index;

    if (sampleCount == 0U) {
        return true;
    }
    if ((input == 0) || (output == 0)) {
        return false;
    }

    for (index = 0U; index < sampleCount; index++) {
        int64_t centered = (int64_t) input[index] - dcValue;
        output[index] = DCRemoval_saturateInt16(centered);
    }
    return true;
}

bool DCRemoval_subtractInt32(const int32_t *input, int32_t *output,
    uint32_t sampleCount, int32_t dcValue)
{
    uint32_t index;

    if (sampleCount == 0U) {
        return true;
    }
    if ((input == 0) || (output == 0)) {
        return false;
    }

    for (index = 0U; index < sampleCount; index++) {
        int64_t centered = (int64_t) input[index] - dcValue;
        output[index] = DCRemoval_saturateInt32(centered);
    }
    return true;
}

bool DCRemoval_processInt16(const int16_t *input, int16_t *output,
    uint32_t sampleCount, int32_t *removedMean)
{
    int32_t mean;

    if ((sampleCount == 0U) ||
        !Mean_calculateInt16(input, sampleCount, &mean)) {
        return false;
    }
    if (!DCRemoval_subtractInt16(
        input, output, sampleCount, mean)) {
        return false;
    }
    if (removedMean != 0) {
        *removedMean = mean;
    }
    return true;
}

bool DCRemoval_processInt32(const int32_t *input, int32_t *output,
    uint32_t sampleCount, int32_t *removedMean)
{
    int32_t mean;

    if ((sampleCount == 0U) ||
        !Mean_calculateInt32(input, sampleCount, &mean)) {
        return false;
    }
    if (!DCRemoval_subtractInt32(
        input, output, sampleCount, mean)) {
        return false;
    }
    if (removedMean != 0) {
        *removedMean = mean;
    }
    return true;
}
