#include "Algorithms/power_analysis/ripple/ripple.h"

#include <limits.h>

static void Ripple_clearResult(Ripple_Result *result)
{
    result->dcMean = 0;
    result->minimum = 0;
    result->maximum = 0;
    result->totalRms = 0U;
    result->rippleRms = 0U;
    result->ripplePeakToPeak = 0U;
    result->positivePeakDeviation = 0U;
    result->negativePeakDeviation = 0U;
    result->rippleFactorQ15 = 0U;
    result->rippleRmsMilliPercent = 0U;
    result->ripplePeakToPeakMilliPercent = 0U;
    result->sampleCount = 0U;
    result->percentageValid = false;
    result->calculationOverflow = false;
    result->valid = false;
}

static uint64_t Ripple_absI64(int64_t value)
{
    return (value >= 0) ? (uint64_t) value :
        (uint64_t) (-(value + 1)) + 1U;
}

static bool Ripple_addI64(
    int64_t first, int64_t second, int64_t *result)
{
    if ((second > 0) && (first > INT64_MAX - second)) {
        return false;
    }
    if ((second < 0) && (first < INT64_MIN - second)) {
        return false;
    }
    *result = first + second;
    return true;
}

static bool Ripple_addU64(
    uint64_t first, uint64_t second, uint64_t *result)
{
    if (UINT64_MAX - first < second) {
        return false;
    }
    *result = first + second;
    return true;
}

static uint64_t Ripple_divideU64Rounded(
    uint64_t numerator, uint64_t denominator)
{
    uint64_t result = numerator / denominator;
    uint64_t remainder = numerator % denominator;

    return (remainder >= denominator / 2U + denominator % 2U) ?
        result + 1U : result;
}

static int64_t Ripple_divideI64Rounded(
    int64_t numerator, uint64_t denominator)
{
    uint64_t rounded = Ripple_divideU64Rounded(
        Ripple_absI64(numerator), denominator);

    if (numerator >= 0) {
        return (int64_t) rounded;
    }
    if (rounded == (UINT64_C(1) << 63)) {
        return INT64_MIN;
    }
    return -(int64_t) rounded;
}

static uint32_t Ripple_integerSquareRoot(uint64_t value)
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

static uint32_t Ripple_ratioScaledSaturated(
    uint64_t numerator, uint64_t denominator, uint32_t scale)
{
    uint64_t whole = numerator / denominator;
    uint64_t remainder = numerator % denominator;
    uint64_t fractional;
    uint64_t result;

    if ((whole != 0U) && (whole > UINT32_MAX / scale)) {
        return UINT32_MAX;
    }
    while ((scale != 0U) &&
           (remainder > UINT64_MAX / scale)) {
        remainder = (remainder + 1U) >> 1;
        denominator = (denominator + 1U) >> 1;
    }
    fractional = (remainder * scale + denominator / 2U) /
        denominator;
    result = whole * scale + fractional;
    return (result > UINT32_MAX) ? UINT32_MAX : (uint32_t) result;
}

bool Ripple_calculate(const int32_t *samples,
    uint32_t sampleCount, Ripple_Result *result)
{
    int64_t sum = 0;
    uint64_t sumSquares = 0U;
    uint64_t sumRippleSquares = 0U;
    uint32_t index;
    int32_t minimum;
    int32_t maximum;
    uint64_t dcMagnitude;

    if (result == 0) {
        return false;
    }
    Ripple_clearResult(result);
    if ((samples == 0) || (sampleCount == 0U)) {
        return false;
    }
    result->sampleCount = sampleCount;
    minimum = samples[0];
    maximum = samples[0];

    for (index = 0U; index < sampleCount; index++) {
        uint64_t magnitude = Ripple_absI64(samples[index]);
        uint64_t square = magnitude * magnitude;

        if (!Ripple_addI64(sum, samples[index], &sum) ||
            !Ripple_addU64(sumSquares, square, &sumSquares)) {
            result->calculationOverflow = true;
            return false;
        }
        if (samples[index] < minimum) {
            minimum = samples[index];
        }
        if (samples[index] > maximum) {
            maximum = samples[index];
        }
    }

    result->dcMean = (int32_t) Ripple_divideI64Rounded(sum, sampleCount);
    result->minimum = minimum;
    result->maximum = maximum;
    result->totalRms = Ripple_integerSquareRoot(
        Ripple_divideU64Rounded(sumSquares, sampleCount));
    result->ripplePeakToPeak = (uint32_t)
        ((int64_t) maximum - minimum);
    result->positivePeakDeviation = (uint32_t)
        ((int64_t) maximum - result->dcMean);
    result->negativePeakDeviation = (uint32_t)
        ((int64_t) result->dcMean - minimum);

    for (index = 0U; index < sampleCount; index++) {
        int64_t difference =
            (int64_t) samples[index] - result->dcMean;
        uint64_t magnitude = Ripple_absI64(difference);
        uint64_t square = magnitude * magnitude;

        if (!Ripple_addU64(
                sumRippleSquares, square, &sumRippleSquares)) {
            result->calculationOverflow = true;
            return false;
        }
    }

    result->rippleRms = Ripple_integerSquareRoot(
        Ripple_divideU64Rounded(sumRippleSquares, sampleCount));
    dcMagnitude = Ripple_absI64(result->dcMean);
    if (dcMagnitude != 0U) {
        result->rippleFactorQ15 = Ripple_ratioScaledSaturated(
            result->rippleRms, dcMagnitude, 32768U);
        result->rippleRmsMilliPercent = Ripple_ratioScaledSaturated(
            result->rippleRms, dcMagnitude, 100000U);
        result->ripplePeakToPeakMilliPercent =
            Ripple_ratioScaledSaturated(
                result->ripplePeakToPeak,
                dcMagnitude, 100000U);
        result->percentageValid = true;
    }

    result->valid = true;
    return true;
}
