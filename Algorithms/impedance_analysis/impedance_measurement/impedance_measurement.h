#ifndef ALGORITHMS_IMPEDANCE_ANALYSIS_IMPEDANCE_MEASUREMENT_H_
#define ALGORITHMS_IMPEDANCE_ANALYSIS_IMPEDANCE_MEASUREMENT_H_

#include <stdbool.h>
#include <stdint.h>

#include "Algorithms/demodulation/synchronous_detection/synchronous_detection.h"

typedef enum {
    IMPEDANCE_NATURE_UNKNOWN = 0,
    IMPEDANCE_NATURE_RESISTIVE,
    IMPEDANCE_NATURE_INDUCTIVE,
    IMPEDANCE_NATURE_CAPACITIVE
} ImpedanceMeasurement_Nature;

/** 同步检波电压、电流计算出的复阻抗。 */
typedef struct {
    uint64_t frequencyMilliHz;
    uint64_t magnitudeMilliOhms;
    int64_t resistanceMilliOhms;
    int64_t reactanceMilliOhms;
    int32_t phaseMilliDegrees;
    ImpedanceMeasurement_Nature nature;
    bool valid;
} ImpedanceMeasurement_Result;

/**
 * 根据同一PLL参考下的电压和电流 I/Q 计算阻抗。
 * 电压使用 mV、电流使用 mA 时，结果自然换算为 mOhm。
 * phaseCorrectionMilliDegrees 用于补偿电压相对电流通道的固定相差。
 * phaseThresholdMilliDegrees 用于把接近0度的负载归类为电阻性。
 */
bool ImpedanceMeasurement_calculate(
    const SynchronousDetection_Result *voltage,
    const SynchronousDetection_Result *current,
    uint64_t frequencyMilliHz,
    int32_t phaseCorrectionMilliDegrees,
    uint32_t phaseThresholdMilliDegrees,
    ImpedanceMeasurement_Result *result);

/** 正电抗时根据频率估算串联等效电感，单位 uH。 */
bool ImpedanceMeasurement_estimateInductanceMicroHenry(
    const ImpedanceMeasurement_Result *result,
    uint64_t *inductanceMicroHenry);

/** 负电抗时根据频率估算串联等效电容，单位 pF。 */
bool ImpedanceMeasurement_estimateCapacitancePicoFarad(
    const ImpedanceMeasurement_Result *result,
    uint64_t *capacitancePicoFarad);

#endif
