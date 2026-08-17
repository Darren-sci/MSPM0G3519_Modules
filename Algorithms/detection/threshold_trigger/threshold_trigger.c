#include "Algorithms/detection/threshold_trigger/threshold_trigger.h"

#include <limits.h>

static int32_t ThresholdTrigger_clampI32(int64_t value)
{
    if (value > INT32_MAX) {
        return INT32_MAX;
    }
    if (value < INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t) value;
}

static uint64_t ThresholdTrigger_crossingQ16(
    uint64_t currentIndex, int32_t previous,
    int32_t current, int32_t threshold)
{
    int64_t span = (int64_t) current - previous;
    int64_t offset = (int64_t) threshold - previous;
    uint64_t fractionQ16 = 0U;
    uint64_t spanMagnitude;
    uint64_t offsetMagnitude;

    if ((span != 0) && (currentIndex != 0U)) {
        spanMagnitude = (span >= 0) ?
            (uint64_t) span : (uint64_t) -span;
        offsetMagnitude = (offset >= 0) ?
            (uint64_t) offset : (uint64_t) -offset;
        fractionQ16 = (offsetMagnitude << 16) / spanMagnitude;
        if (fractionQ16 > 65536U) {
            fractionQ16 = 65536U;
        }
    }
    return ((currentIndex - 1U) << 16) + fractionQ16;
}

static bool ThresholdTrigger_edgeEnabled(
    ThresholdTrigger_Edge configured,
    ThresholdTrigger_Edge candidate)
{
    return (((uint32_t) configured & (uint32_t) candidate) != 0U);
}

bool ThresholdTrigger_init(ThresholdTrigger_State *state,
    const ThresholdTrigger_Config *config)
{
    if ((state == 0) || (config == 0) ||
        ((config->edge != THRESHOLD_TRIGGER_RISING) &&
         (config->edge != THRESHOLD_TRIGGER_FALLING) &&
         (config->edge != THRESHOLD_TRIGGER_EITHER))) {
        return false;
    }

    state->config = *config;
    state->initialized = true;
    return ThresholdTrigger_reset(state);
}

bool ThresholdTrigger_reset(ThresholdTrigger_State *state)
{
    if ((state == 0) || !state->initialized) {
        return false;
    }
    state->previousSample = 0;
    state->nextSampleIndex = 0U;
    state->holdoffRemaining = 0U;
    state->risingArmed = false;
    state->fallingArmed = false;
    state->havePrevious = false;
    return true;
}

bool ThresholdTrigger_processSample(ThresholdTrigger_State *state,
    int32_t sample, bool *triggered,
    ThresholdTrigger_Event *event)
{
    int32_t lower;
    int32_t upper;
    uint64_t currentIndex;
    ThresholdTrigger_Edge detectedEdge = THRESHOLD_TRIGGER_RISING;
    bool detected = false;

    if ((state == 0) || !state->initialized ||
        (triggered == 0) || (event == 0) ||
        (state->nextSampleIndex > (UINT64_MAX >> 16))) {
        return false;
    }
    *triggered = false;
    lower = ThresholdTrigger_clampI32(
        (int64_t) state->config.threshold - state->config.hysteresis);
    upper = ThresholdTrigger_clampI32(
        (int64_t) state->config.threshold + state->config.hysteresis);
    currentIndex = state->nextSampleIndex;

    if (!state->havePrevious) {
        state->risingArmed = sample <= lower;
        state->fallingArmed = sample >= upper;
        state->previousSample = sample;
        state->havePrevious = true;
        state->nextSampleIndex++;
        return true;
    }

    if (state->holdoffRemaining != 0U) {
        state->holdoffRemaining--;
    } else if (state->risingArmed &&
        ThresholdTrigger_edgeEnabled(
            state->config.edge, THRESHOLD_TRIGGER_RISING) &&
        (state->previousSample < state->config.threshold) &&
        (sample >= state->config.threshold)) {
        detected = true;
        detectedEdge = THRESHOLD_TRIGGER_RISING;
    } else if (state->fallingArmed &&
        ThresholdTrigger_edgeEnabled(
            state->config.edge, THRESHOLD_TRIGGER_FALLING) &&
        (state->previousSample > state->config.threshold) &&
        (sample <= state->config.threshold)) {
        detected = true;
        detectedEdge = THRESHOLD_TRIGGER_FALLING;
    }

    if (detected) {
        event->sampleIndexQ16 = ThresholdTrigger_crossingQ16(
            currentIndex, state->previousSample,
            sample, state->config.threshold);
        event->previousSample = state->previousSample;
        event->currentSample = sample;
        event->edge = detectedEdge;
        state->holdoffRemaining = state->config.holdoffSamples;
        if (detectedEdge == THRESHOLD_TRIGGER_RISING) {
            state->risingArmed = false;
        } else {
            state->fallingArmed = false;
        }
        *triggered = true;
    }

    if (sample <= lower) {
        state->risingArmed = true;
    }
    if (sample >= upper) {
        state->fallingArmed = true;
    }
    state->previousSample = sample;
    state->nextSampleIndex++;
    return true;
}

bool ThresholdTrigger_processBlock(ThresholdTrigger_State *state,
    const int32_t *samples, uint32_t sampleCount,
    ThresholdTrigger_Event *events, uint32_t eventCapacity,
    uint32_t *eventCount, bool *eventOverflow)
{
    uint32_t index;

    if ((state == 0) || !state->initialized ||
        (eventCount == 0) || (eventOverflow == 0) ||
        ((eventCapacity != 0U) && (events == 0))) {
        return false;
    }
    *eventCount = 0U;
    *eventOverflow = false;
    if (sampleCount == 0U) {
        return true;
    }
    if (samples == 0) {
        return false;
    }

    for (index = 0U; index < sampleCount; index++) {
        ThresholdTrigger_Event event;
        bool triggered;

        if (!ThresholdTrigger_processSample(
                state, samples[index], &triggered, &event)) {
            return false;
        }
        if (triggered) {
            if (*eventCount < eventCapacity) {
                events[*eventCount] = event;
                (*eventCount)++;
            } else {
                *eventOverflow = true;
            }
        }
    }
    return true;
}
