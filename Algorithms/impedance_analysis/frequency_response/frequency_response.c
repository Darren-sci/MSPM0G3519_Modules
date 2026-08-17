#include "Algorithms/impedance_analysis/frequency_response/frequency_response.h"

#include <limits.h>

static int32_t FrequencyResponse_wrapPhase(int64_t phaseMilliDegrees)
{
    while (phaseMilliDegrees > 180000) {
        phaseMilliDegrees -= 360000;
    }
    while (phaseMilliDegrees <= -180000) {
        phaseMilliDegrees += 360000;
    }
    return (int32_t) phaseMilliDegrees;
}

static uint32_t FrequencyResponse_ratioScaledSaturated(
    uint64_t numerator, uint64_t denominator, uint32_t scale)
{
    uint64_t whole = numerator / denominator;
    uint64_t remainder = numerator % denominator;
    uint64_t result;

    if ((whole != 0U) && (whole > UINT32_MAX / scale)) {
        return UINT32_MAX;
    }
    result = whole * scale +
        (remainder * scale + denominator / 2U) / denominator;
    return (result > UINT32_MAX) ? UINT32_MAX : (uint32_t) result;
}

static int64_t FrequencyResponse_log2Q20(uint64_t value)
{
    uint8_t highestBit = 0U;
    uint64_t temporary = value;
    uint64_t normalized;
    uint32_t fraction = 0U;
    uint8_t iteration;

    while (temporary > 1U) {
        temporary >>= 1;
        highestBit++;
    }

    if (highestBit >= 31U) {
        normalized = value >> (highestBit - 31U);
    } else {
        normalized = value << (31U - highestBit);
    }

    for (iteration = 0U; iteration < 20U; iteration++) {
        normalized = (normalized * normalized) >> 31;
        fraction <<= 1;
        if (normalized >= (UINT64_C(2) << 31)) {
            normalized >>= 1;
            fraction |= 1U;
        }
    }
    return (int64_t) highestBit * (INT64_C(1) << 20) + fraction;
}

bool FrequencyResponse_calculate(
    const SynchronousDetection_Result *input,
    const SynchronousDetection_Result *output,
    uint64_t frequencyMilliHz,
    int32_t phaseCorrectionMilliDegrees,
    FrequencyResponse_Result *result)
{
    int64_t logRatioQ20;
    int64_t milliDecibels;
    int64_t phase;
    int64_t delayNumerator;
    uint64_t delayDenominator;

    if (result == 0) {
        return false;
    }
    result->frequencyMilliHz = 0U;
    result->inputAmplitude = 0U;
    result->outputAmplitude = 0U;
    result->gainQ15 = 0U;
    result->gainMilliPercent = 0U;
    result->gainMilliDecibels = INT32_MIN;
    result->phaseMilliDegrees = 0;
    result->equivalentDelayNanoSeconds = 0;
    result->valid = false;

    if ((input == 0) || (output == 0) ||
        !input->valid || !output->valid ||
        input->saturated || output->saturated ||
        (input->amplitude == 0U) ||
        (output->amplitude == 0U) ||
        (frequencyMilliHz == 0U) ||
        (frequencyMilliHz > (uint64_t) INT64_MAX / 360U)) {
        return false;
    }

    result->frequencyMilliHz = frequencyMilliHz;
    result->inputAmplitude = input->amplitude;
    result->outputAmplitude = output->amplitude;
    result->gainQ15 = FrequencyResponse_ratioScaledSaturated(
        output->amplitude, input->amplitude, 32768U);
    result->gainMilliPercent = FrequencyResponse_ratioScaledSaturated(
        output->amplitude, input->amplitude, 100000U);

    logRatioQ20 = FrequencyResponse_log2Q20(output->amplitude) -
                  FrequencyResponse_log2Q20(input->amplitude);
    milliDecibels = (logRatioQ20 * 6021 +
        ((logRatioQ20 >= 0) ?
            (INT64_C(1) << 19) : -(INT64_C(1) << 19))) /
        (INT64_C(1) << 20);
    if (milliDecibels > INT32_MAX) {
        milliDecibels = INT32_MAX;
    } else if (milliDecibels < INT32_MIN) {
        milliDecibels = INT32_MIN;
    }
    result->gainMilliDecibels = (int32_t) milliDecibels;

    phase = (int64_t) output->phaseMilliDegrees -
        input->phaseMilliDegrees + phaseCorrectionMilliDegrees;
    result->phaseMilliDegrees = FrequencyResponse_wrapPhase(phase);
    delayNumerator = -(int64_t) result->phaseMilliDegrees *
        INT64_C(1000000000);
    delayDenominator = frequencyMilliHz * 360U;
    if (delayNumerator >= 0) {
        result->equivalentDelayNanoSeconds =
            (delayNumerator + (int64_t) delayDenominator / 2) /
            (int64_t) delayDenominator;
    } else {
        result->equivalentDelayNanoSeconds =
            -((-delayNumerator + (int64_t) delayDenominator / 2) /
              (int64_t) delayDenominator);
    }
    result->valid = true;
    return true;
}
