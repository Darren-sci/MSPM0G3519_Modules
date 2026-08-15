#include "Algorithms/filters/moving_average/moving_average.h"

static bool MovingAverage_isValid(const MovingAverage_Filter *filter)
{
    return (filter != 0) &&
           (filter->state != 0) &&
           (filter->windowSize != 0U) &&
           (filter->writeIndex < filter->windowSize) &&
           (filter->sampleCount <= filter->windowSize);
}

static int16_t MovingAverage_divideRounded(
    int64_t sum, uint16_t divisor)
{
    int64_t halfDivisor = (int64_t) divisor / 2;

    if (sum >= 0) {
        return (int16_t) ((sum + halfDivisor) / divisor);
    }
    return (int16_t) -((-sum + halfDivisor) / divisor);
}

bool MovingAverage_init(MovingAverage_Filter *filter,
    int16_t *state, uint16_t windowSize)
{
    if ((filter == 0) || (state == 0) || (windowSize == 0U)) {
        return false;
    }

    filter->state = state;
    filter->windowSize = windowSize;
    filter->runningSum = 0;
    filter->writeIndex = 0U;
    filter->sampleCount = 0U;
    MovingAverage_reset(filter);
    return true;
}

void MovingAverage_reset(MovingAverage_Filter *filter)
{
    uint16_t index;

    if ((filter == 0) || (filter->state == 0) ||
        (filter->windowSize == 0U)) {
        return;
    }

    for (index = 0U; index < filter->windowSize; index++) {
        filter->state[index] = 0;
    }
    filter->runningSum = 0;
    filter->writeIndex = 0U;
    filter->sampleCount = 0U;
}

int16_t MovingAverage_processSample(
    MovingAverage_Filter *filter, int16_t input)
{
    uint16_t divisor;

    if (!MovingAverage_isValid(filter)) {
        return 0;
    }

    if (filter->sampleCount < filter->windowSize) {
        filter->runningSum += input;
        filter->sampleCount++;
    } else {
        filter->runningSum -= filter->state[filter->writeIndex];
        filter->runningSum += input;
    }

    filter->state[filter->writeIndex] = input;
    filter->writeIndex++;
    if (filter->writeIndex >= filter->windowSize) {
        filter->writeIndex = 0U;
    }

    divisor = filter->sampleCount;
    return MovingAverage_divideRounded(filter->runningSum, divisor);
}

bool MovingAverage_processBlock(MovingAverage_Filter *filter,
    const int16_t *input, int16_t *output, uint32_t sampleCount)
{
    uint32_t index;

    if (!MovingAverage_isValid(filter)) {
        return false;
    }
    if (sampleCount == 0U) {
        return true;
    }
    if ((input == 0) || (output == 0)) {
        return false;
    }

    for (index = 0U; index < sampleCount; index++) {
        output[index] =
            MovingAverage_processSample(filter, input[index]);
    }
    return true;
}
