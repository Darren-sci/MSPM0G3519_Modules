#ifndef ALGORITHMS_MEASUREMENTS_PHASE_PHASE_H_
#define ALGORITHMS_MEASUREMENTS_PHASE_PHASE_H_

#include <stdbool.h>
#include <stdint.h>

/** 相位测量配置。通道时间偏移使用 Q16.16 采样点。 */
typedef struct {
    int16_t referenceThreshold;
    int16_t targetThreshold;
    uint16_t referenceHysteresis;
    uint16_t targetHysteresis;
    int32_t targetTimeOffsetQ16;
} Phase_Config;

/**
 * 相位结果。delayQ16 为正表示目标通道边沿晚于参考通道；
 * phaseMilliDegrees 为正表示目标超前参考。
 */
typedef struct {
    int64_t delayQ16;
    int32_t phaseMilliDegrees;
    uint64_t averagePeriodQ16;
    uint32_t pairCount;
} Phase_Result;

/**
 * 根据两路同步采样数据的上升沿测量相位。
 *
 * 两路信号必须同频且每块至少包含两个参考上升沿。目标通道相对参考通道
 * 的实际采样时刻差通过 targetTimeOffsetQ16 提供。
 */
bool Phase_calculate(const int16_t *reference, const int16_t *target,
    uint32_t sampleCount, const Phase_Config *config,
    Phase_Result *result);

#endif
