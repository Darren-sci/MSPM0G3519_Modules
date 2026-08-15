#include "Algorithms/measurements/frequency/frequency.h"

#include <limits.h>

#define FREQUENCY_Q16_ONE    (65536ULL)

static int32_t Frequency_lowerLevel(const Frequency_Estimator *estimator)
{
    int32_t level = (int32_t) estimator->threshold - estimator->hysteresis;
    return (level < INT16_MIN) ? INT16_MIN : level;
}

static int32_t Frequency_upperLevel(const Frequency_Estimator *estimator)
{
    int32_t level = (int32_t) estimator->threshold + estimator->hysteresis;
    return (level > INT16_MAX) ? INT16_MAX : level;
}

static uint64_t Frequency_crossingPositionQ16(uint64_t currentIndex,
    int16_t previous, int16_t current, int16_t threshold)
{
    int32_t sampleSpan = (int32_t) current - previous;
    int32_t thresholdOffset = (int32_t) threshold - previous;
    uint64_t fractionQ16 = 0U;

    if (sampleSpan > 0) {
        fractionQ16 = ((uint64_t) (uint32_t) thresholdOffset << 16) /
            (uint32_t) sampleSpan;
        if (fractionQ16 > FREQUENCY_Q16_ONE) {
            fractionQ16 = FREQUENCY_Q16_ONE;
        }
    }
    return ((currentIndex - 1U) << 16) + fractionQ16;
}

static bool Frequency_acceptCrossing(
    Frequency_Estimator *estimator, uint64_t crossingQ16)
{
    if (estimator->haveLastCrossing) {
        uint64_t periodQ16 = crossingQ16 - estimator->lastCrossingQ16;

        if ((periodQ16 == 0U) ||
            (UINT64_MAX - estimator->periodSumQ16 < periodQ16) ||
            (estimator->periodCount == UINT32_MAX)) {
            return false;
        }
        estimator->periodSumQ16 += periodQ16;
        estimator->periodCount++;
    }
    estimator->lastCrossingQ16 = crossingQ16;
    estimator->haveLastCrossing = true;
    return true;
}

bool Frequency_init(Frequency_Estimator *estimator,
    uint32_t sampleRateHz, int16_t threshold, uint16_t hysteresis)
{
    if ((estimator == 0) || (sampleRateHz == 0U)) {
        return false;
    }
    estimator->sampleRateHz = sampleRateHz;
    estimator->threshold = threshold;
    estimator->hysteresis = hysteresis;
    Frequency_reset(estimator);
    return true;
}

void Frequency_reset(Frequency_Estimator *estimator)
{
    if (estimator == 0) {
        return;
    }
    estimator->previousSample = 0;
    estimator->nextSampleIndex = 0U;
    estimator->pendingCrossingQ16 = 0U;
    estimator->lastCrossingQ16 = 0U;
    estimator->periodSumQ16 = 0U;
    estimator->periodCount = 0U;
    estimator->initialized = false;
    estimator->lowArmed = false;
    estimator->pendingRising = false;
    estimator->haveLastCrossing = false;
}

bool Frequency_processSample(
    Frequency_Estimator *estimator, int16_t input)
{
    uint64_t currentIndex;
    int32_t lower;
    int32_t upper;

    if ((estimator == 0) || (estimator->sampleRateHz == 0U) ||
        (estimator->nextSampleIndex >= (UINT64_MAX >> 16))) {
        return false;
    }

    currentIndex = estimator->nextSampleIndex;
    lower = Frequency_lowerLevel(estimator);
    upper = Frequency_upperLevel(estimator);

    if (!estimator->initialized) {
        estimator->previousSample = input;
        estimator->lowArmed = ((int32_t) input <= lower);
        estimator->initialized = true;
        estimator->nextSampleIndex++;
        return true;
    }

    if (estimator->lowArmed) {
        if (!estimator->pendingRising &&
            (estimator->previousSample < estimator->threshold) &&
            (input >= estimator->threshold)) {
            estimator->pendingCrossingQ16 =
                Frequency_crossingPositionQ16(currentIndex,
                    estimator->previousSample, input,
                    estimator->threshold);
            estimator->pendingRising = true;
        }

        if (estimator->pendingRising) {
            if ((int32_t) input >= upper) {
                if (!Frequency_acceptCrossing(
                    estimator, estimator->pendingCrossingQ16)) {
                    return false;
                }
                estimator->lowArmed = false;
                estimator->pendingRising = false;
            } else if (input < estimator->threshold) {
                estimator->pendingRising = false;
            }
        }
    } else if ((int32_t) input <= lower) {
        estimator->lowArmed = true;
        estimator->pendingRising = false;
    }

    estimator->previousSample = input;
    estimator->nextSampleIndex++;
    return true;
}

bool Frequency_processBlock(Frequency_Estimator *estimator,
    const int16_t *input, uint32_t sampleCount)
{
    uint32_t index;

    if (estimator == 0) {
        return false;
    }
    if (sampleCount == 0U) {
        return true;
    }
    if (input == 0) {
        return false;
    }

    for (index = 0U; index < sampleCount; index++) {
        if (!Frequency_processSample(estimator, input[index])) {
            return false;
        }
    }
    return true;
}

bool Frequency_getResult(
    const Frequency_Estimator *estimator, Frequency_Result *result)
{
    uint64_t averagePeriodQ16;
    uint64_t numerator;
    uint64_t frequencyMilliHz;

    if ((estimator == 0) || (result == 0) ||
        (estimator->sampleRateHz == 0U) ||
        (estimator->periodCount == 0U)) {
        return false;
    }

    averagePeriodQ16 =
        (estimator->periodSumQ16 + estimator->periodCount / 2U) /
        estimator->periodCount;
    if (averagePeriodQ16 == 0U) {
        return false;
    }

    numerator = (uint64_t) estimator->sampleRateHz *
        1000U * FREQUENCY_Q16_ONE;
    frequencyMilliHz =
        (numerator + averagePeriodQ16 / 2U) / averagePeriodQ16;

    result->averagePeriodQ16 = averagePeriodQ16;
    result->frequencyMilliHz = (frequencyMilliHz > UINT32_MAX) ?
        UINT32_MAX : (uint32_t) frequencyMilliHz;
    result->periodCount = estimator->periodCount;
    return true;
}

bool Frequency_calculate(const int16_t *input, uint32_t sampleCount,
    uint32_t sampleRateHz, int16_t threshold, uint16_t hysteresis,
    Frequency_Result *result)
{
    Frequency_Estimator estimator;

    return Frequency_init(
               &estimator, sampleRateHz, threshold, hysteresis) &&
           Frequency_processBlock(&estimator, input, sampleCount) &&
           Frequency_getResult(&estimator, result);
}
