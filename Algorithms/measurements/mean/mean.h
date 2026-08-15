#ifndef ALGORITHMS_MEASUREMENTS_MEAN_MEAN_H_
#define ALGORITHMS_MEASUREMENTS_MEAN_MEAN_H_

#include <stdbool.h>
#include <stdint.h>

/** 可跨多个数据块使用的平均值累加器。 */
typedef struct {
    int64_t sum;
    uint32_t sampleCount;
} Mean_Accumulator;

/** 清空累加和与样本数量。 */
void Mean_reset(Mean_Accumulator *accumulator);

/** 加入一个 int32_t 样本；样本计数已满时返回 false。 */
bool Mean_addSample(Mean_Accumulator *accumulator, int32_t sample);

/** 加入一块 int16_t 数据；sampleCount 可以为 0。 */
bool Mean_addBlockInt16(Mean_Accumulator *accumulator,
    const int16_t *input, uint32_t sampleCount);

/** 加入一块 int32_t 数据；sampleCount 可以为 0。 */
bool Mean_addBlockInt32(Mean_Accumulator *accumulator,
    const int32_t *input, uint32_t sampleCount);

/** 取得四舍五入后的平均值；没有样本时返回 false。 */
bool Mean_get(const Mean_Accumulator *accumulator, int32_t *mean);

/** 返回累加器当前包含的样本数量。 */
uint32_t Mean_getSampleCount(const Mean_Accumulator *accumulator);

/** 一次性计算一块 int16_t 数据的平均值。 */
bool Mean_calculateInt16(
    const int16_t *input, uint32_t sampleCount, int32_t *mean);

/** 一次性计算一块 int32_t 数据的平均值。 */
bool Mean_calculateInt32(
    const int32_t *input, uint32_t sampleCount, int32_t *mean);

#endif
