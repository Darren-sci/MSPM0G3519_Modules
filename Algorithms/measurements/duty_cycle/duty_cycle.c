#include "Algorithms/measurements/duty_cycle/duty_cycle.h"

#include <limits.h>

#define DUTY_CYCLE_Q16_ONE    (65536ULL)

static int32_t DutyCycle_lowerLevel(const DutyCycle_Meter *meter)
{
    int32_t level = (int32_t) meter->threshold - meter->hysteresis;
    return (level < INT16_MIN) ? INT16_MIN : level;
}

static int32_t DutyCycle_upperLevel(const DutyCycle_Meter *meter)
{
    int32_t level = (int32_t) meter->threshold + meter->hysteresis;
    return (level > INT16_MAX) ? INT16_MAX : level;
}

static uint64_t DutyCycle_crossingPositionQ16(uint64_t currentIndex,
    int16_t previous, int16_t current, int16_t threshold)
{
    int32_t span = (int32_t) current - previous;
    int32_t offset = (int32_t) threshold - previous;
    uint64_t fractionQ16;

    if (span > 0) {
        fractionQ16 = ((uint64_t) (uint32_t) offset << 16) /
            (uint32_t) span;
    } else if (span < 0) {
        fractionQ16 = ((uint64_t) (uint32_t) (-offset) << 16) /
            (uint32_t) (-span);
    } else {
        fractionQ16 = 0U;
    }
    if (fractionQ16 > DUTY_CYCLE_Q16_ONE) {
        fractionQ16 = DUTY_CYCLE_Q16_ONE;
    }
    return ((currentIndex - 1U) << 16) + fractionQ16;
}

static bool DutyCycle_acceptRising(
    DutyCycle_Meter *meter, uint64_t crossingQ16)
{
    if (meter->haveRising && meter->haveFalling) {
        uint64_t periodQ16 = crossingQ16 - meter->lastRisingQ16;
        uint64_t highTimeQ16 = meter->fallingQ16 - meter->lastRisingQ16;

        if ((periodQ16 == 0U) || (highTimeQ16 > periodQ16) ||
            (UINT64_MAX - meter->periodSumQ16 < periodQ16) ||
            (UINT64_MAX - meter->highTimeSumQ16 < highTimeQ16) ||
            (meter->cycleCount == UINT32_MAX)) {
            return false;
        }
        meter->periodSumQ16 += periodQ16;
        meter->highTimeSumQ16 += highTimeQ16;
        meter->cycleCount++;
    }

    meter->lastRisingQ16 = crossingQ16;
    meter->haveRising = true;
    meter->haveFalling = false;
    return true;
}

bool DutyCycle_init(DutyCycle_Meter *meter,
    int16_t threshold, uint16_t hysteresis)
{
    if (meter == 0) {
        return false;
    }
    meter->threshold = threshold;
    meter->hysteresis = hysteresis;
    DutyCycle_reset(meter);
    return true;
}

void DutyCycle_reset(DutyCycle_Meter *meter)
{
    if (meter == 0) {
        return;
    }
    meter->previousSample = 0;
    meter->nextSampleIndex = 0U;
    meter->pendingCrossingQ16 = 0U;
    meter->lastRisingQ16 = 0U;
    meter->fallingQ16 = 0U;
    meter->highTimeSumQ16 = 0U;
    meter->periodSumQ16 = 0U;
    meter->cycleCount = 0U;
    meter->initialized = false;
    meter->highState = false;
    meter->pendingEdge = false;
    meter->haveRising = false;
    meter->haveFalling = false;
}

bool DutyCycle_processSample(DutyCycle_Meter *meter, int16_t input)
{
    uint64_t currentIndex;
    int32_t lower;
    int32_t upper;

    if ((meter == 0) ||
        (meter->nextSampleIndex >= (UINT64_MAX >> 16))) {
        return false;
    }

    currentIndex = meter->nextSampleIndex;
    lower = DutyCycle_lowerLevel(meter);
    upper = DutyCycle_upperLevel(meter);

    if (!meter->initialized) {
        meter->previousSample = input;
        meter->highState = (input >= meter->threshold);
        meter->initialized = true;
        meter->nextSampleIndex++;
        return true;
    }

    if (!meter->highState) {
        if (!meter->pendingEdge &&
            (meter->previousSample < meter->threshold) &&
            (input >= meter->threshold)) {
            meter->pendingCrossingQ16 =
                DutyCycle_crossingPositionQ16(currentIndex,
                    meter->previousSample, input, meter->threshold);
            meter->pendingEdge = true;
        }

        if (meter->pendingEdge) {
            if ((int32_t) input >= upper) {
                if (!DutyCycle_acceptRising(
                    meter, meter->pendingCrossingQ16)) {
                    return false;
                }
                meter->highState = true;
                meter->pendingEdge = false;
            } else if (input < meter->threshold) {
                meter->pendingEdge = false;
            }
        }
    } else {
        if (!meter->pendingEdge &&
            (meter->previousSample > meter->threshold) &&
            (input <= meter->threshold)) {
            meter->pendingCrossingQ16 =
                DutyCycle_crossingPositionQ16(currentIndex,
                    meter->previousSample, input, meter->threshold);
            meter->pendingEdge = true;
        }

        if (meter->pendingEdge) {
            if ((int32_t) input <= lower) {
                if (meter->haveRising && !meter->haveFalling &&
                    (meter->pendingCrossingQ16 > meter->lastRisingQ16)) {
                    meter->fallingQ16 = meter->pendingCrossingQ16;
                    meter->haveFalling = true;
                }
                meter->highState = false;
                meter->pendingEdge = false;
            } else if (input > meter->threshold) {
                meter->pendingEdge = false;
            }
        }
    }

    meter->previousSample = input;
    meter->nextSampleIndex++;
    return true;
}

bool DutyCycle_processBlock(DutyCycle_Meter *meter,
    const int16_t *input, uint32_t sampleCount)
{
    uint32_t index;

    if (meter == 0) {
        return false;
    }
    if (sampleCount == 0U) {
        return true;
    }
    if (input == 0) {
        return false;
    }

    for (index = 0U; index < sampleCount; index++) {
        if (!DutyCycle_processSample(meter, input[index])) {
            return false;
        }
    }
    return true;
}

bool DutyCycle_getResult(
    const DutyCycle_Meter *meter, DutyCycle_Result *result)
{
    uint64_t dutyPermille;

    if ((meter == 0) || (result == 0) ||
        (meter->cycleCount == 0U) || (meter->periodSumQ16 == 0U)) {
        return false;
    }

    dutyPermille = (meter->highTimeSumQ16 * 1000U +
        meter->periodSumQ16 / 2U) / meter->periodSumQ16;
    if (dutyPermille > 1000U) {
        dutyPermille = 1000U;
    }

    result->dutyPermille = (uint16_t) dutyPermille;
    result->averageHighTimeQ16 =
        (meter->highTimeSumQ16 + meter->cycleCount / 2U) /
        meter->cycleCount;
    result->averagePeriodQ16 =
        (meter->periodSumQ16 + meter->cycleCount / 2U) /
        meter->cycleCount;
    result->cycleCount = meter->cycleCount;
    return true;
}

bool DutyCycle_calculate(const int16_t *input, uint32_t sampleCount,
    int16_t threshold, uint16_t hysteresis, DutyCycle_Result *result)
{
    DutyCycle_Meter meter;

    return DutyCycle_init(&meter, threshold, hysteresis) &&
           DutyCycle_processBlock(&meter, input, sampleCount) &&
           DutyCycle_getResult(&meter, result);
}
