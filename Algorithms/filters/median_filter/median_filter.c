#include "Algorithms/filters/median_filter/median_filter.h"

static bool MedianFilter_isValid(const MedianFilter_Filter *filter)
{
    return (filter != 0) &&
           (filter->state != 0) &&
           (filter->scratch != 0) &&
           (filter->windowSize != 0U) &&
           ((filter->windowSize & 1U) != 0U) &&
           (filter->writeIndex < filter->windowSize) &&
           (filter->sampleCount <= filter->windowSize);
}

static void MedianFilter_sort(int16_t *values, uint16_t count)
{
    uint16_t index;

    /* 小窗口使用插入排序，代码短且不需要额外内存。 */
    for (index = 1U; index < count; index++) {
        int16_t value = values[index];
        uint16_t position = index;

        while ((position > 0U) &&
               (values[position - 1U] > value)) {
            values[position] = values[position - 1U];
            position--;
        }
        values[position] = value;
    }
}

bool MedianFilter_init(MedianFilter_Filter *filter,
    int16_t *state, int16_t *scratch, uint16_t windowSize)
{
    if ((filter == 0) || (state == 0) || (scratch == 0) ||
        (windowSize == 0U) || ((windowSize & 1U) == 0U)) {
        return false;
    }

    filter->state = state;
    filter->scratch = scratch;
    filter->windowSize = windowSize;
    filter->writeIndex = 0U;
    filter->sampleCount = 0U;
    MedianFilter_reset(filter);
    return true;
}

void MedianFilter_reset(MedianFilter_Filter *filter)
{
    uint16_t index;

    if ((filter == 0) || (filter->state == 0) ||
        (filter->scratch == 0) || (filter->windowSize == 0U)) {
        return;
    }

    for (index = 0U; index < filter->windowSize; index++) {
        filter->state[index] = 0;
        filter->scratch[index] = 0;
    }
    filter->writeIndex = 0U;
    filter->sampleCount = 0U;
}

int16_t MedianFilter_processSample(
    MedianFilter_Filter *filter, int16_t input)
{
    uint16_t index;

    if (!MedianFilter_isValid(filter)) {
        return 0;
    }

    filter->state[filter->writeIndex] = input;
    filter->writeIndex++;
    if (filter->writeIndex >= filter->windowSize) {
        filter->writeIndex = 0U;
    }
    if (filter->sampleCount < filter->windowSize) {
        filter->sampleCount++;
    }

    for (index = 0U; index < filter->sampleCount; index++) {
        filter->scratch[index] = filter->state[index];
    }
    MedianFilter_sort(filter->scratch, filter->sampleCount);
    return filter->scratch[filter->sampleCount / 2U];
}

bool MedianFilter_processBlock(MedianFilter_Filter *filter,
    const int16_t *input, int16_t *output, uint32_t sampleCount)
{
    uint32_t index;

    if (!MedianFilter_isValid(filter)) {
        return false;
    }
    if (sampleCount == 0U) {
        return true;
    }
    if ((input == 0) || (output == 0)) {
        return false;
    }

    for (index = 0U; index < sampleCount; index++) {
        output[index] = MedianFilter_processSample(filter, input[index]);
    }
    return true;
}
