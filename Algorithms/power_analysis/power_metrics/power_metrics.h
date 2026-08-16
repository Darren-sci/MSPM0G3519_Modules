#ifndef ALGORITHMS_POWER_ANALYSIS_POWER_METRICS_H_
#define ALGORITHMS_POWER_ANALYSIS_POWER_METRICS_H_

#include <stdbool.h>
#include <stdint.h>

/** 一块同步电压、电流数据的主要电气指标。 */
typedef struct {
    int64_t activePower;
    int64_t dcPower;
    int64_t acActivePower;
    uint64_t apparentPower;
    uint64_t reactivePowerMagnitude;
    int32_t meanVoltage;
    int32_t meanCurrent;
    uint32_t voltageRms;
    uint32_t currentRms;
    uint32_t voltageAcRms;
    uint32_t currentAcRms;
    uint32_t voltagePeakAbs;
    uint32_t currentPeakAbs;
    int32_t powerFactorQ15;
    uint32_t voltageCrestFactorQ15;
    uint32_t currentCrestFactorQ15;
    uint32_t sampleCount;
    bool calculationOverflow;
    bool valid;
} PowerMetrics_Result;

/**
 * 从同步采集的电压、电流数组计算功率指标。
 *
 * 电压和电流可以使用任意一致的整数单位。例如输入 mV、mA 时，功率
 * 输出单位为 uW；输入 Q15、Q15 时，功率为 Q30。sampleCount 必须大于0。
 */
bool PowerMetrics_calculate(const int32_t *voltage,
    const int32_t *current, uint32_t sampleCount,
    PowerMetrics_Result *result);

/**
 * 根据正的输入、输出有功功率计算效率。
 * efficiencyQ15 中32768表示100%；efficiencyMilliPercent 中100000表示
 * 100.000%。不需要某个输出时可传空指针。
 */
bool PowerMetrics_calculateEfficiency(int64_t inputActivePower,
    int64_t outputActivePower, uint32_t *efficiencyQ15,
    uint32_t *efficiencyMilliPercent);

#endif
