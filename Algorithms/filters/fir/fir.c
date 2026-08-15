#include "Algorithms/filters/fir/fir.h"

#include <limits.h>

#define FIR_Q15_SCALE          (32768LL)
#define FIR_Q15_ROUNDING       (FIR_Q15_SCALE / 2LL)

static bool FIR_isValid(const FIR_Filter *filter)
{
    return (filter != 0) &&
           (filter->coefficients != 0) &&
           (filter->state != 0) &&
           (filter->tapCount != 0U) &&
           (filter->writeIndex < filter->tapCount);
}

static int16_t FIR_accumulatorToQ15(int64_t accumulator)
{
    int64_t rounded;

    /* 分开处理正负数，避免依赖编译器对负数右移的具体实现。 */
    if (accumulator >= 0) {
        rounded = (accumulator + FIR_Q15_ROUNDING) / FIR_Q15_SCALE;
    } else {
        rounded = -((-accumulator + FIR_Q15_ROUNDING) / FIR_Q15_SCALE);
    }

    if (rounded > INT16_MAX) {
        return INT16_MAX;
    }
    if (rounded < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t) rounded;
}

bool FIR_init(FIR_Filter *filter, const int16_t *coefficients,
    int16_t *state, uint16_t tapCount)
{
    if ((filter == 0) || (coefficients == 0) ||
        (state == 0) || (tapCount == 0U)) {
        return false;
    }

    filter->coefficients = coefficients;
    filter->state = state;
    filter->tapCount = tapCount;
    filter->writeIndex = 0U;
    FIR_reset(filter);
    return true;
}

void FIR_reset(FIR_Filter *filter)
{
    uint16_t index;

    if ((filter == 0) || (filter->state == 0) ||
        (filter->tapCount == 0U)) {
        return;
    }

    for (index = 0U; index < filter->tapCount; index++) {
        filter->state[index] = 0;
    }
    filter->writeIndex = 0U;
}

int16_t FIR_processSample(FIR_Filter *filter, int16_t input)
{
    uint16_t tap;
    uint16_t stateIndex;
    int64_t accumulator = 0;

    if (!FIR_isValid(filter)) {
        return 0;
    }

    filter->state[filter->writeIndex] = input;
    stateIndex = filter->writeIndex;

    for (tap = 0U; tap < filter->tapCount; tap++) {
        accumulator +=
            (int32_t) filter->coefficients[tap] *
            (int32_t) filter->state[stateIndex];

        if (stateIndex == 0U) {
            stateIndex = (uint16_t) (filter->tapCount - 1U);
        } else {
            stateIndex--;
        }
    }

    filter->writeIndex++;
    if (filter->writeIndex >= filter->tapCount) {
        filter->writeIndex = 0U;
    }

    return FIR_accumulatorToQ15(accumulator);
}

bool FIR_processBlock(FIR_Filter *filter, const int16_t *input,
    int16_t *output, uint32_t sampleCount)
{
    uint32_t index;

    if (!FIR_isValid(filter)) {
        return false;
    }
    if (sampleCount == 0U) {
        return true;
    }
    if ((input == 0) || (output == 0)) {
        return false;
    }

    for (index = 0U; index < sampleCount; index++) {
        output[index] = FIR_processSample(filter, input[index]);
    }
    return true;
}
