#include "Algorithms/detection/overrange_detection/overrange_detection.h"

#include <limits.h>

static int32_t OverrangeDetection_clampI32(int64_t value)
{
    if (value > INT32_MAX) {
        return INT32_MAX;
    }
    if (value < INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t) value;
}

static void OverrangeDetection_incrementSaturated(uint32_t *value)
{
    if (*value != UINT32_MAX) {
        (*value)++;
    }
}

bool OverrangeDetection_init(OverrangeDetection_State *state,
    const OverrangeDetection_Config *config)
{
    if ((state == 0) || (config == 0) ||
        (config->lowerLimit >= config->upperLimit) ||
        ((uint64_t) config->releaseHysteresis >
         (uint64_t) ((int64_t) config->upperLimit -
                     config->lowerLimit)) ||
        (config->consecutiveAssertSamples == 0U) ||
        (config->consecutiveReleaseSamples == 0U)) {
        return false;
    }

    state->config = *config;
    state->initialized = true;
    return OverrangeDetection_reset(state);
}

bool OverrangeDetection_reset(OverrangeDetection_State *state)
{
    if ((state == 0) || !state->initialized) {
        return false;
    }
    state->lowRunLength = 0U;
    state->highRunLength = 0U;
    state->lowReleaseRunLength = 0U;
    state->highReleaseRunLength = 0U;
    state->totalSampleCount = 0U;
    state->lowActive = false;
    state->highActive = false;
    return true;
}

bool OverrangeDetection_processBlock(
    OverrangeDetection_State *state,
    const int32_t *samples, uint32_t sampleCount,
    OverrangeDetection_Result *result)
{
    int32_t lowReleaseLevel;
    int32_t highReleaseLevel;
    uint32_t index;

    if ((state == 0) || !state->initialized || (samples == 0) ||
        (sampleCount == 0U) || (result == 0)) {
        return false;
    }

    result->minimum = samples[0];
    result->maximum = samples[0];
    result->lowSampleCount = 0U;
    result->highSampleCount = 0U;
    result->longestLowRun = state->lowRunLength;
    result->longestHighRun = state->highRunLength;
    result->firstLowIndex = UINT32_MAX;
    result->firstHighIndex = UINT32_MAX;
    result->lowAssertedThisBlock = false;
    result->highAssertedThisBlock = false;
    result->lowReleasedThisBlock = false;
    result->highReleasedThisBlock = false;
    result->lowActive = state->lowActive;
    result->highActive = state->highActive;
    result->valid = false;

    lowReleaseLevel = OverrangeDetection_clampI32(
        (int64_t) state->config.lowerLimit +
        state->config.releaseHysteresis);
    highReleaseLevel = OverrangeDetection_clampI32(
        (int64_t) state->config.upperLimit -
        state->config.releaseHysteresis);

    for (index = 0U; index < sampleCount; index++) {
        int32_t sample = samples[index];

        if (sample < result->minimum) {
            result->minimum = sample;
        }
        if (sample > result->maximum) {
            result->maximum = sample;
        }

        if (sample <= state->config.lowerLimit) {
            if (result->firstLowIndex == UINT32_MAX) {
                result->firstLowIndex = index;
            }
            OverrangeDetection_incrementSaturated(
                &result->lowSampleCount);
            OverrangeDetection_incrementSaturated(
                &state->lowRunLength);
            state->lowReleaseRunLength = 0U;
            if (state->lowRunLength > result->longestLowRun) {
                result->longestLowRun = state->lowRunLength;
            }
            if (!state->lowActive &&
                (state->lowRunLength >=
                 state->config.consecutiveAssertSamples)) {
                state->lowActive = true;
                result->lowAssertedThisBlock = true;
            }
        } else {
            state->lowRunLength = 0U;
            if (state->lowActive && (sample >= lowReleaseLevel)) {
                OverrangeDetection_incrementSaturated(
                    &state->lowReleaseRunLength);
                if (state->lowReleaseRunLength >=
                    state->config.consecutiveReleaseSamples) {
                    state->lowActive = false;
                    state->lowReleaseRunLength = 0U;
                    result->lowReleasedThisBlock = true;
                }
            } else {
                state->lowReleaseRunLength = 0U;
            }
        }

        if (sample >= state->config.upperLimit) {
            if (result->firstHighIndex == UINT32_MAX) {
                result->firstHighIndex = index;
            }
            OverrangeDetection_incrementSaturated(
                &result->highSampleCount);
            OverrangeDetection_incrementSaturated(
                &state->highRunLength);
            state->highReleaseRunLength = 0U;
            if (state->highRunLength > result->longestHighRun) {
                result->longestHighRun = state->highRunLength;
            }
            if (!state->highActive &&
                (state->highRunLength >=
                 state->config.consecutiveAssertSamples)) {
                state->highActive = true;
                result->highAssertedThisBlock = true;
            }
        } else {
            state->highRunLength = 0U;
            if (state->highActive && (sample <= highReleaseLevel)) {
                OverrangeDetection_incrementSaturated(
                    &state->highReleaseRunLength);
                if (state->highReleaseRunLength >=
                    state->config.consecutiveReleaseSamples) {
                    state->highActive = false;
                    state->highReleaseRunLength = 0U;
                    result->highReleasedThisBlock = true;
                }
            } else {
                state->highReleaseRunLength = 0U;
            }
        }

        if (state->totalSampleCount != UINT64_MAX) {
            state->totalSampleCount++;
        }
    }

    result->lowActive = state->lowActive;
    result->highActive = state->highActive;
    result->valid = true;
    return true;
}
