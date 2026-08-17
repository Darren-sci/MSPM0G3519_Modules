#ifndef ALGORITHMS_IMPEDANCE_ANALYSIS_FREQUENCY_RESPONSE_H_
#define ALGORITHMS_IMPEDANCE_ANALYSIS_FREQUENCY_RESPONSE_H_

#include <stdbool.h>
#include <stdint.h>

#include "Algorithms/demodulation/synchronous_detection/synchronous_detection.h"

/** 单个扫频点的增益、相位与等效延迟。 */
typedef struct {
    uint64_t frequencyMilliHz;
    uint32_t inputAmplitude;
    uint32_t outputAmplitude;
    uint32_t gainQ15;
    uint32_t gainMilliPercent;
    int32_t gainMilliDecibels;
    int32_t phaseMilliDegrees;
    int64_t equivalentDelayNanoSeconds;
    bool valid;
} FrequencyResponse_Result;

/**
 * 根据同一PLL参考下的输入、输出同步检波结果计算一个频响点。
 * phaseCorrectionMilliDegrees 用于补偿ADC通道、模拟前端或线缆固定相差。
 */
bool FrequencyResponse_calculate(
    const SynchronousDetection_Result *input,
    const SynchronousDetection_Result *output,
    uint64_t frequencyMilliHz,
    int32_t phaseCorrectionMilliDegrees,
    FrequencyResponse_Result *result);

#endif
