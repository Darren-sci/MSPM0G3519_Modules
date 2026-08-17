#ifndef ALGORITHMS_DETECTION_THRESHOLD_TRIGGER_H_
#define ALGORITHMS_DETECTION_THRESHOLD_TRIGGER_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    THRESHOLD_TRIGGER_RISING = 1,
    THRESHOLD_TRIGGER_FALLING = 2,
    THRESHOLD_TRIGGER_EITHER = 3
} ThresholdTrigger_Edge;

typedef struct {
    int32_t threshold;
    uint32_t hysteresis;
    uint32_t holdoffSamples;
    ThresholdTrigger_Edge edge;
} ThresholdTrigger_Config;

typedef struct {
    ThresholdTrigger_Config config;
    int32_t previousSample;
    uint64_t nextSampleIndex;
    uint32_t holdoffRemaining;
    bool risingArmed;
    bool fallingArmed;
    bool havePrevious;
    bool initialized;
} ThresholdTrigger_State;

/** sampleIndexQ16 是从状态启动开始计算的亚采样触发位置。 */
typedef struct {
    uint64_t sampleIndexQ16;
    int32_t previousSample;
    int32_t currentSample;
    ThresholdTrigger_Edge edge;
} ThresholdTrigger_Event;

bool ThresholdTrigger_init(ThresholdTrigger_State *state,
    const ThresholdTrigger_Config *config);

bool ThresholdTrigger_reset(ThresholdTrigger_State *state);

/** 处理一个连续采样。triggered 返回本采样是否产生事件。 */
bool ThresholdTrigger_processSample(ThresholdTrigger_State *state,
    int32_t sample, bool *triggered,
    ThresholdTrigger_Event *event);

/**
 * 处理数据块并收集事件。事件数量超过容量时继续更新状态，并通过
 * eventOverflow 报告；eventCount 是实际写入数组的数量。
 */
bool ThresholdTrigger_processBlock(ThresholdTrigger_State *state,
    const int32_t *samples, uint32_t sampleCount,
    ThresholdTrigger_Event *events, uint32_t eventCapacity,
    uint32_t *eventCount, bool *eventOverflow);

#endif
