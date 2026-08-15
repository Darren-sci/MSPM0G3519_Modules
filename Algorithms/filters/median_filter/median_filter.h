#ifndef ALGORITHMS_FILTERS_MEDIAN_FILTER_MEDIAN_FILTER_H_
#define ALGORITHMS_FILTERS_MEDIAN_FILTER_MEDIAN_FILTER_H_

#include <stdbool.h>
#include <stdint.h>

/** 中值滤波器实例。 */
typedef struct {
    int16_t *state;
    int16_t *scratch;
    uint16_t windowSize;
    uint16_t writeIndex;
    uint16_t sampleCount;
} MedianFilter_Filter;

/**
 * 初始化中值滤波器并清空状态。
 *
 * state 和 scratch 均由调用者提供，各自至少需要容纳 windowSize 个
 * int16_t。windowSize 必须是大于 0 的奇数。
 */
bool MedianFilter_init(MedianFilter_Filter *filter,
    int16_t *state, int16_t *scratch, uint16_t windowSize);

/** 清除历史样本，但保留窗口和缓冲区配置。 */
void MedianFilter_reset(MedianFilter_Filter *filter);

/**
 * 处理一个有符号 16 位采样点。
 *
 * 窗口填满前只对已经收到的样本排序，并取其中间位置的值。
 */
int16_t MedianFilter_processSample(
    MedianFilter_Filter *filter, int16_t input);

/** 处理一块连续数据；input 与 output 可以指向同一缓冲区。 */
bool MedianFilter_processBlock(MedianFilter_Filter *filter,
    const int16_t *input, int16_t *output, uint32_t sampleCount);

#endif
