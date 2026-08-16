#ifndef ALGORITHMS_PREPROCESSING_DC_REMOVAL_DC_REMOVAL_H_
#define ALGORITHMS_PREPROCESSING_DC_REMOVAL_DC_REMOVAL_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * 计算一块 int16_t 数据的平均值并将其从每个采样中减去。
 *
 * input 与 output 可以指向同一缓冲区。removedMean 返回实际减去的整数
 * 平均值；不需要时可以传入空指针。sampleCount 必须大于 0。
 */
bool DCRemoval_processInt16(const int16_t *input, int16_t *output,
    uint32_t sampleCount, int32_t *removedMean);

/** int32_t 版本的自动块平均值去除，支持原地处理。 */
bool DCRemoval_processInt32(const int32_t *input, int32_t *output,
    uint32_t sampleCount, int32_t *removedMean);

/**
 * 从一块 int16_t 数据中减去调用者指定的直流值。
 *
 * 计算使用更宽的中间类型，超出 int16_t 范围的结果会被饱和。
 */
bool DCRemoval_subtractInt16(const int16_t *input, int16_t *output,
    uint32_t sampleCount, int32_t dcValue);

/** 从一块 int32_t 数据中减去指定直流值，并进行 int32_t 饱和。 */
bool DCRemoval_subtractInt32(const int32_t *input, int32_t *output,
    uint32_t sampleCount, int32_t dcValue);

#endif
