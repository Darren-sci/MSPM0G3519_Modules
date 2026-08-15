#include "Algorithms/measurements/phase/phase.h"

#include <limits.h>

#define PHASE_Q16_ONE             (65536ULL)
#define PHASE_MILLIDEGREES_CYCLE  (360000LL)

typedef struct {
    const int16_t *input;
    uint32_t sampleCount;
    uint32_t nextIndex;
    int16_t threshold;
    int32_t lower;
    int32_t upper;
    int16_t previous;
    uint64_t pendingQ16;
    bool initialized;
    bool lowArmed;
    bool pendingRising;
} Phase_EdgeScanner;

static int32_t Phase_clampLevel(int32_t level)
{
    if (level < INT16_MIN) {
        return INT16_MIN;
    }
    if (level > INT16_MAX) {
        return INT16_MAX;
    }
    return level;
}

static void Phase_scannerInit(Phase_EdgeScanner *scanner,
    const int16_t *input, uint32_t sampleCount,
    int16_t threshold, uint16_t hysteresis)
{
    scanner->input = input;
    scanner->sampleCount = sampleCount;
    scanner->nextIndex = 0U;
    scanner->threshold = threshold;
    scanner->lower = Phase_clampLevel((int32_t) threshold - hysteresis);
    scanner->upper = Phase_clampLevel((int32_t) threshold + hysteresis);
    scanner->previous = 0;
    scanner->pendingQ16 = 0U;
    scanner->initialized = false;
    scanner->lowArmed = false;
    scanner->pendingRising = false;
}

static uint64_t Phase_crossingPositionQ16(uint32_t currentIndex,
    int16_t previous, int16_t current, int16_t threshold)
{
    int32_t span = (int32_t) current - previous;
    int32_t offset = (int32_t) threshold - previous;
    uint64_t fractionQ16 = 0U;

    if (span > 0) {
        fractionQ16 = ((uint64_t) (uint32_t) offset << 16) /
            (uint32_t) span;
        if (fractionQ16 > PHASE_Q16_ONE) {
            fractionQ16 = PHASE_Q16_ONE;
        }
    }
    return (((uint64_t) currentIndex - 1U) << 16) + fractionQ16;
}

static bool Phase_nextRising(
    Phase_EdgeScanner *scanner, uint64_t *crossingQ16)
{
    while (scanner->nextIndex < scanner->sampleCount) {
        uint32_t currentIndex = scanner->nextIndex;
        int16_t current = scanner->input[currentIndex];

        scanner->nextIndex++;
        if (!scanner->initialized) {
            scanner->previous = current;
            scanner->lowArmed = ((int32_t) current <= scanner->lower);
            scanner->initialized = true;
            continue;
        }

        if (scanner->lowArmed) {
            if (!scanner->pendingRising &&
                (scanner->previous < scanner->threshold) &&
                (current >= scanner->threshold)) {
                scanner->pendingQ16 = Phase_crossingPositionQ16(
                    currentIndex, scanner->previous,
                    current, scanner->threshold);
                scanner->pendingRising = true;
            }

            if (scanner->pendingRising) {
                if ((int32_t) current >= scanner->upper) {
                    *crossingQ16 = scanner->pendingQ16;
                    scanner->lowArmed = false;
                    scanner->pendingRising = false;
                    scanner->previous = current;
                    return true;
                }
                if (current < scanner->threshold) {
                    scanner->pendingRising = false;
                }
            }
        } else if ((int32_t) current <= scanner->lower) {
            scanner->lowArmed = true;
            scanner->pendingRising = false;
        }
        scanner->previous = current;
    }
    return false;
}

static int64_t Phase_wrapDelay(int64_t delayQ16, uint64_t periodQ16)
{
    int64_t period = (int64_t) periodQ16;
    int64_t halfPeriod = period / 2;

    while (delayQ16 > halfPeriod) {
        delayQ16 -= period;
    }
    while (delayQ16 <= -halfPeriod) {
        delayQ16 += period;
    }
    return delayQ16;
}

static int64_t Phase_divideSignedRounded(
    int64_t numerator, uint64_t denominator)
{
    int64_t half = (int64_t) (denominator / 2U);

    if (numerator >= 0) {
        return (numerator + half) / denominator;
    }
    return -((-numerator + half) / denominator);
}

bool Phase_calculate(const int16_t *reference, const int16_t *target,
    uint32_t sampleCount, const Phase_Config *config,
    Phase_Result *result)
{
    Phase_EdgeScanner referenceScanner;
    Phase_EdgeScanner targetScanner;
    uint64_t previousReference;
    uint64_t referenceCrossing;
    uint64_t periodSum = 0U;
    uint32_t periodCount = 0U;
    uint64_t averagePeriod;
    uint64_t targetPrevious = 0U;
    uint64_t targetNext = 0U;
    bool haveTargetPrevious = false;
    bool haveTargetNext;
    int64_t delaySum = 0;
    uint32_t pairCount = 0U;
    int64_t averageDelay;
    int64_t phase;

    if ((reference == 0) || (target == 0) || (config == 0) ||
        (result == 0) || (sampleCount < 3U)) {
        return false;
    }

    Phase_scannerInit(&referenceScanner, reference, sampleCount,
        config->referenceThreshold, config->referenceHysteresis);
    if (!Phase_nextRising(&referenceScanner, &previousReference)) {
        return false;
    }
    while (Phase_nextRising(&referenceScanner, &referenceCrossing)) {
        uint64_t period = referenceCrossing - previousReference;

        if ((period == 0U) || (UINT64_MAX - periodSum < period) ||
            (periodCount == UINT32_MAX)) {
            return false;
        }
        periodSum += period;
        periodCount++;
        previousReference = referenceCrossing;
    }
    if (periodCount == 0U) {
        return false;
    }
    averagePeriod = (periodSum + periodCount / 2U) / periodCount;

    Phase_scannerInit(&referenceScanner, reference, sampleCount,
        config->referenceThreshold, config->referenceHysteresis);
    Phase_scannerInit(&targetScanner, target, sampleCount,
        config->targetThreshold, config->targetHysteresis);
    haveTargetNext = Phase_nextRising(&targetScanner, &targetNext);

    while (Phase_nextRising(&referenceScanner, &referenceCrossing)) {
        uint64_t chosenTarget;
        int64_t delay;

        while (haveTargetNext && (targetNext < referenceCrossing)) {
            targetPrevious = targetNext;
            haveTargetPrevious = true;
            haveTargetNext = Phase_nextRising(&targetScanner, &targetNext);
        }

        if (haveTargetPrevious && haveTargetNext) {
            uint64_t previousDistance =
                referenceCrossing - targetPrevious;
            uint64_t nextDistance = targetNext - referenceCrossing;
            chosenTarget = (previousDistance <= nextDistance) ?
                targetPrevious : targetNext;
        } else if (haveTargetPrevious) {
            chosenTarget = targetPrevious;
        } else if (haveTargetNext) {
            chosenTarget = targetNext;
        } else {
            break;
        }

        delay = (int64_t) chosenTarget - (int64_t) referenceCrossing +
            config->targetTimeOffsetQ16;
        delay = Phase_wrapDelay(delay, averagePeriod);

        if (((delay > 0) && (delaySum > INT64_MAX - delay)) ||
            ((delay < 0) && (delaySum < INT64_MIN - delay)) ||
            (pairCount == UINT32_MAX)) {
            return false;
        }
        delaySum += delay;
        pairCount++;
    }

    if (pairCount == 0U) {
        return false;
    }
    averageDelay = Phase_divideSignedRounded(delaySum, pairCount);
    phase = Phase_divideSignedRounded(
        -averageDelay * PHASE_MILLIDEGREES_CYCLE,
        averagePeriod);

    if (phase > 180000) {
        phase = 180000;
    } else if (phase < -180000) {
        phase = -180000;
    }

    result->delayQ16 = averageDelay;
    result->phaseMilliDegrees = (int32_t) phase;
    result->averagePeriodQ16 = averagePeriod;
    result->pairCount = pairCount;
    return true;
}
