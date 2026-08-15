#include "Algorithms/measurements/rms/rms.h"

#include <limits.h>

static int32_t RMS_divideSignedRounded(
    int64_t numerator, uint32_t denominator)
{
    int64_t halfDenominator = (int64_t) denominator / 2;

    if (numerator >= 0) {
        return (int32_t)
            ((numerator + halfDenominator) / denominator);
    }
    return (int32_t)
        -((-numerator + halfDenominator) / denominator);
}

static uint32_t RMS_integerSquareRoot(uint64_t value)
{
    uint64_t remainder = value;
    uint64_t root = 0U;
    uint64_t bit = (uint64_t) 1U << 62;

    while (bit > remainder) {
        bit >>= 2;
    }

    while (bit != 0U) {
        if (remainder >= root + bit) {
            remainder -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return (uint32_t) root;
}

static uint32_t RMS_fromSumSquares(
    uint64_t sumSquares, uint32_t sampleCount)
{
    uint64_t meanSquareFloor = sumSquares / sampleCount;
    uint32_t root = RMS_integerSquareRoot(meanSquareFloor);
    uint64_t thresholdFour =
        4U * (uint64_t) root * root + 4U * root + 1U;

    /* 直接比较真实均方值与半整数阈值，避免先除法再开方的双重舍入。 */
    if ((4U * sumSquares) >=
        ((uint64_t) sampleCount * thresholdFour)) {
        root++;
    }
    return root;
}

void RMS_reset(RMS_Accumulator *accumulator)
{
    if (accumulator == 0) {
        return;
    }
    accumulator->sumSquares = 0U;
    accumulator->sampleCount = 0U;
}

bool RMS_addSample(RMS_Accumulator *accumulator, int16_t sample)
{
    int32_t value = sample;

    if ((accumulator == 0) ||
        (accumulator->sampleCount == UINT32_MAX)) {
        return false;
    }
    accumulator->sumSquares += (uint64_t) (value * value);
    accumulator->sampleCount++;
    return true;
}

bool RMS_addBlock(RMS_Accumulator *accumulator,
    const int16_t *input, uint32_t sampleCount)
{
    uint32_t index;

    if ((accumulator == 0) ||
        (sampleCount > UINT32_MAX - accumulator->sampleCount)) {
        return false;
    }
    if (sampleCount == 0U) {
        return true;
    }
    if (input == 0) {
        return false;
    }

    for (index = 0U; index < sampleCount; index++) {
        int32_t value = input[index];
        accumulator->sumSquares += (uint64_t) (value * value);
    }
    accumulator->sampleCount += sampleCount;
    return true;
}

bool RMS_get(const RMS_Accumulator *accumulator, uint32_t *rms)
{
    if ((accumulator == 0) || (rms == 0) ||
        (accumulator->sampleCount == 0U)) {
        return false;
    }

    *rms = RMS_fromSumSquares(
        accumulator->sumSquares, accumulator->sampleCount);
    return true;
}

uint32_t RMS_getSampleCount(const RMS_Accumulator *accumulator)
{
    return (accumulator != 0) ? accumulator->sampleCount : 0U;
}

bool RMS_calculate(
    const int16_t *input, uint32_t sampleCount, uint32_t *rms)
{
    RMS_Accumulator accumulator;

    RMS_reset(&accumulator);
    return RMS_addBlock(&accumulator, input, sampleCount) &&
           RMS_get(&accumulator, rms);
}

bool RMS_calculateAC(const int16_t *input, uint32_t sampleCount,
    uint32_t *rms, int32_t *mean)
{
    uint32_t index;
    int64_t sum = 0;
    int32_t blockMean;
    uint64_t sumSquares = 0U;

    if ((input == 0) || (rms == 0) || (sampleCount == 0U)) {
        return false;
    }

    for (index = 0U; index < sampleCount; index++) {
        sum += input[index];
    }
    blockMean = RMS_divideSignedRounded(sum, sampleCount);

    for (index = 0U; index < sampleCount; index++) {
        int64_t difference = (int64_t) input[index] - blockMean;
        sumSquares += (uint64_t) (difference * difference);
    }

    *rms = RMS_fromSumSquares(sumSquares, sampleCount);
    if (mean != 0) {
        *mean = blockMean;
    }
    return true;
}
