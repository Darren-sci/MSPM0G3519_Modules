#ifndef ALGORITHMS_MEASUREMENTS_DUTY_CYCLE_DUTY_CYCLE_H_
#define ALGORITHMS_MEASUREMENTS_DUTY_CYCLE_DUTY_CYCLE_H_

#include <stdbool.h>
#include <stdint.h>

/** 占空比结果；Q16.16 时间的单位是采样点。 */
typedef struct {
    uint16_t dutyPermille;
    uint64_t averageHighTimeQ16;
    uint64_t averagePeriodQ16;
    uint32_t cycleCount;
} DutyCycle_Result;

/** 基于带滞回阈值边沿的流式占空比测量器。 */
typedef struct {
    int16_t threshold;
    uint16_t hysteresis;
    int16_t previousSample;
    uint64_t nextSampleIndex;
    uint64_t pendingCrossingQ16;
    uint64_t lastRisingQ16;
    uint64_t fallingQ16;
    uint64_t highTimeSumQ16;
    uint64_t periodSumQ16;
    uint32_t cycleCount;
    bool initialized;
    bool highState;
    bool pendingEdge;
    bool haveRising;
    bool haveFalling;
} DutyCycle_Meter;

/** 初始化并清空占空比测量器。 */
bool DutyCycle_init(DutyCycle_Meter *meter,
    int16_t threshold, uint16_t hysteresis);

/** 清除历史和累计周期，但保留阈值及滞回配置。 */
void DutyCycle_reset(DutyCycle_Meter *meter);

/** 加入一个采样点。 */
bool DutyCycle_processSample(DutyCycle_Meter *meter, int16_t input);

/** 加入一块连续采样；sampleCount 可以为 0。 */
bool DutyCycle_processBlock(DutyCycle_Meter *meter,
    const int16_t *input, uint32_t sampleCount);

/** 取得平均高电平时间、周期和千分比占空比。 */
bool DutyCycle_getResult(
    const DutyCycle_Meter *meter, DutyCycle_Result *result);

/** 对单个数据块进行一次完整占空比测量。 */
bool DutyCycle_calculate(const int16_t *input, uint32_t sampleCount,
    int16_t threshold, uint16_t hysteresis, DutyCycle_Result *result);

#endif
