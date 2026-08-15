#ifndef ALGORITHMS_MEASUREMENTS_RMS_RMS_H_
#define ALGORITHMS_MEASUREMENTS_RMS_RMS_H_

#include <stdbool.h>
#include <stdint.h>

/** 可跨多个数据块使用的 int16_t 总 RMS 累加器。 */
typedef struct {
    uint64_t sumSquares;
    uint32_t sampleCount;
} RMS_Accumulator;

/** 清空平方和与样本数量。 */
void RMS_reset(RMS_Accumulator *accumulator);

/** 加入一个 int16_t 样本；样本计数已满时返回 false。 */
bool RMS_addSample(RMS_Accumulator *accumulator, int16_t sample);

/** 加入一块 int16_t 数据；sampleCount 可以为 0。 */
bool RMS_addBlock(RMS_Accumulator *accumulator,
    const int16_t *input, uint32_t sampleCount);

/** 取得四舍五入后的总 RMS；没有样本时返回 false。 */
bool RMS_get(const RMS_Accumulator *accumulator, uint32_t *rms);

/** 返回累加器当前包含的样本数量。 */
uint32_t RMS_getSampleCount(const RMS_Accumulator *accumulator);

/** 一次性计算 int16_t 数据的总 RMS，结果包含直流分量。 */
bool RMS_calculate(
    const int16_t *input, uint32_t sampleCount, uint32_t *rms);

/**
 * 一次性计算 int16_t 数据去除块平均值后的交流 RMS。
 *
 * mean 返回本次计算采用的整数平均值；不需要时可以传入空指针。
 */
bool RMS_calculateAC(const int16_t *input, uint32_t sampleCount,
    uint32_t *rms, int32_t *mean);

#endif
