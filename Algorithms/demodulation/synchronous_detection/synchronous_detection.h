#ifndef ALGORITHMS_DEMODULATION_SYNCHRONOUS_DETECTION_H_
#define ALGORITHMS_DEMODULATION_SYNCHRONOUS_DETECTION_H_

#include <stdbool.h>
#include <stdint.h>

/** 同步检波的同相、正交、幅值和相位结果。 */
typedef struct {
    int32_t inPhase;
    int32_t quadrature;
    uint32_t amplitude;
    int32_t phaseMilliDegrees;
    uint32_t processedSampleCount;
    bool saturated;
    bool stable;
    bool valid;
} SynchronousDetection_Result;

/** 连续一阶低通同步检波器状态。 */
typedef struct {
    int64_t filteredInPhase;
    int64_t filteredQuadrature;
    uint32_t processedSampleCount;
    uint32_t settlingSampleCount;
    uint32_t minimumAmplitude;
    uint16_t filterCoefficientQ15;
    bool initialized;
} SynchronousDetection_State;

/**
 * 对完整数据块进行相干积分。
 * input 使用校准后的任意有符号整数单位；referenceSineQ15 和
 * referenceCosineQ15 使用 Q15，通常直接来自 PLL。返回 I/Q 与幅值保持
 * input 的单位，phaseMilliDegrees 范围为 -180000～180000。
 */
bool SynchronousDetection_calculateBlock(
    const int32_t *input,
    const int16_t *referenceSineQ15,
    const int16_t *referenceCosineQ15,
    uint32_t sampleCount,
    SynchronousDetection_Result *result);

/** 初始化连续同步检波器。 */
bool SynchronousDetection_init(SynchronousDetection_State *state,
    uint16_t filterCoefficientQ15,
    uint32_t minimumAmplitude,
    uint32_t settlingSampleCount);

/** 清除连续低通历史和稳定计数。 */
bool SynchronousDetection_reset(SynchronousDetection_State *state);

/** 用一对正交参考处理一个采样。 */
bool SynchronousDetection_processSample(
    SynchronousDetection_State *state,
    int32_t input,
    int16_t referenceSineQ15,
    int16_t referenceCosineQ15,
    SynchronousDetection_Result *result);

/** 连续处理数据块并返回最后一个采样后的结果。 */
bool SynchronousDetection_processBlock(
    SynchronousDetection_State *state,
    const int32_t *input,
    const int16_t *referenceSineQ15,
    const int16_t *referenceCosineQ15,
    uint32_t sampleCount,
    SynchronousDetection_Result *lastResult);

/** 通用定点 atan2，返回 -180000～180000 mdeg。 */
int32_t SynchronousDetection_atan2MilliDegrees(
    int64_t y, int64_t x);

#endif
