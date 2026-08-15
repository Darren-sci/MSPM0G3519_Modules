#include "Algorithms/filters/iir/iir.h"

#include <limits.h>

#define IIR_Q30_SCALE       (1073741824LL)
#define IIR_Q30_ROUNDING    (IIR_Q30_SCALE / 2LL)

static bool IIR_isValid(const IIR_Filter *filter)
{
    return (filter != 0) &&
           (filter->coefficients != 0) &&
           (filter->state != 0) &&
           (filter->sectionCount != 0U);
}

static int16_t IIR_accumulatorToQ15(int64_t accumulator)
{
    int64_t rounded;

    /* Q15 与 Q30 相乘得到 Q45，除以 2^30 后恢复为 Q15。 */
    if (accumulator >= 0) {
        rounded = (accumulator + IIR_Q30_ROUNDING) / IIR_Q30_SCALE;
    } else {
        rounded = -((-accumulator + IIR_Q30_ROUNDING) / IIR_Q30_SCALE);
    }

    if (rounded > INT16_MAX) {
        return INT16_MAX;
    }
    if (rounded < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t) rounded;
}

static int16_t IIR_processBiquad(
    const IIR_BiquadCoefficients *coefficients,
    IIR_BiquadState *state, int16_t input)
{
    int64_t accumulator;
    int16_t output;

    accumulator =
        (int64_t) coefficients->b0 * input +
        (int64_t) coefficients->b1 * state->x1 +
        (int64_t) coefficients->b2 * state->x2 -
        (int64_t) coefficients->a1 * state->y1 -
        (int64_t) coefficients->a2 * state->y2;

    output = IIR_accumulatorToQ15(accumulator);

    state->x2 = state->x1;
    state->x1 = input;
    state->y2 = state->y1;
    state->y1 = output;
    return output;
}

bool IIR_init(IIR_Filter *filter,
    const IIR_BiquadCoefficients *coefficients,
    IIR_BiquadState *state, uint16_t sectionCount)
{
    if ((filter == 0) || (coefficients == 0) ||
        (state == 0) || (sectionCount == 0U)) {
        return false;
    }

    filter->coefficients = coefficients;
    filter->state = state;
    filter->sectionCount = sectionCount;
    IIR_reset(filter);
    return true;
}

void IIR_reset(IIR_Filter *filter)
{
    uint16_t section;

    if ((filter == 0) || (filter->state == 0) ||
        (filter->sectionCount == 0U)) {
        return;
    }

    for (section = 0U; section < filter->sectionCount; section++) {
        filter->state[section].x1 = 0;
        filter->state[section].x2 = 0;
        filter->state[section].y1 = 0;
        filter->state[section].y2 = 0;
    }
}

int16_t IIR_processSample(IIR_Filter *filter, int16_t input)
{
    uint16_t section;
    int16_t output = input;

    if (!IIR_isValid(filter)) {
        return 0;
    }

    for (section = 0U; section < filter->sectionCount; section++) {
        output = IIR_processBiquad(
            &filter->coefficients[section],
            &filter->state[section], output);
    }
    return output;
}

bool IIR_processBlock(IIR_Filter *filter, const int16_t *input,
    int16_t *output, uint32_t sampleCount)
{
    uint32_t index;

    if (!IIR_isValid(filter)) {
        return false;
    }
    if (sampleCount == 0U) {
        return true;
    }
    if ((input == 0) || (output == 0)) {
        return false;
    }

    for (index = 0U; index < sampleCount; index++) {
        output[index] = IIR_processSample(filter, input[index]);
    }
    return true;
}
