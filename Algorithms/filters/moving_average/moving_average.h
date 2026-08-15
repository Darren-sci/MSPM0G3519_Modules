#ifndef ALGORITHMS_FILTERS_MOVING_AVERAGE_MOVING_AVERAGE_H_
#define ALGORITHMS_FILTERS_MOVING_AVERAGE_MOVING_AVERAGE_H_

#include <stdbool.h>
#include <stdint.h>

/** 滑动平均滤波器实例。 */
typedef struct {
    int16_t *state;
    int64_t runningSum;
    uint16_t windowSize;
    uint16_t writeIndex;
    uint16_t sampleCount;
} MovingAverage_Filter;

/**
 * 初始化滑动平均滤波器并清空状态。
 *
 * state 由调用者提供，至少需要容纳 windowSize 个 int16_t，并且在滤波器
 * 使用期间始终有效。windowSize 必须大于 0。
 */
bool MovingAverage_init(MovingAverage_Filter *filter,
    int16_t *state, uint16_t windowSize);

/** 清除历史样本和累加和，但保留窗口配置。 */
void MovingAverage_reset(MovingAverage_Filter *filter);

/**
 * 处理一个有符号 16 位采样点。
 *
 * 窗口填满前使用已经收到的样本数量作为除数，避免初始零值拉低输出。
 */
int16_t MovingAverage_processSample(
    MovingAverage_Filter *filter, int16_t input);

/** 处理一块连续数据；input 与 output 可以指向同一缓冲区。 */
bool MovingAverage_processBlock(MovingAverage_Filter *filter,
    const int16_t *input, int16_t *output, uint32_t sampleCount);

#endif
