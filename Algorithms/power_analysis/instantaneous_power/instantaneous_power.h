#ifndef ALGORITHMS_POWER_ANALYSIS_INSTANTANEOUS_POWER_H_
#define ALGORITHMS_POWER_ANALYSIS_INSTANTANEOUS_POWER_H_

#include <stdbool.h>
#include <stdint.h>

/** 跨数据块累计有符号瞬时功率，用于平均有功功率。 */
typedef struct {
    int64_t sumPower;
    uint64_t sampleCount;
    bool saturated;
} InstantaneousPower_Accumulator;

/**
 * 计算一个同步电压、电流采样的瞬时功率。
 * 输入为 mV、mA 时输出单位为 uW；输入均为 Q15 时输出为 Q30。
 */
int64_t InstantaneousPower_calculateSample(
    int32_t voltage, int32_t current);

/**
 * 逐点计算一块同步采样的瞬时功率。三个缓冲区不能重叠；sampleCount
 * 可以为 0。
 */
bool InstantaneousPower_calculateBlock(
    const int32_t *voltage, const int32_t *current,
    int64_t *power, uint32_t sampleCount);

/** 清空累计器。 */
bool InstantaneousPower_resetAccumulator(
    InstantaneousPower_Accumulator *accumulator);

/**
 * 把一对同步采样加入累计器。若总和或计数饱和则返回 false，并永久设置
 * saturated，直到重新调用 reset。
 */
bool InstantaneousPower_accumulateSample(
    InstantaneousPower_Accumulator *accumulator,
    int32_t voltage, int32_t current);

/** 把一块同步采样加入累计器。 */
bool InstantaneousPower_accumulateBlock(
    InstantaneousPower_Accumulator *accumulator,
    const int32_t *voltage, const int32_t *current,
    uint32_t sampleCount);

/**
 * 返回累计瞬时功率的算术平均值，即有功功率；采用正负对称四舍五入。
 */
bool InstantaneousPower_getAverage(
    const InstantaneousPower_Accumulator *accumulator,
    int64_t *averagePower);

/**
 * 根据固定采样率积分能量，结果单位为“功率输入单位·秒”。例如瞬时功率
 * 使用 uW 时，输出就是 uJ；使用 mW 时，输出就是 mJ。
 */
bool InstantaneousPower_getEnergy(
    const InstantaneousPower_Accumulator *accumulator,
    uint32_t sampleRateHz, int64_t *energy);

#endif
