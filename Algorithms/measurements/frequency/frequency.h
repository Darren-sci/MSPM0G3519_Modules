#ifndef ALGORITHMS_MEASUREMENTS_FREQUENCY_FREQUENCY_H_
#define ALGORITHMS_MEASUREMENTS_FREQUENCY_FREQUENCY_H_

#include <stdbool.h>
#include <stdint.h>

/** 频率测量结果；Q16.16 周期的单位是采样点。 */
typedef struct {
    uint64_t averagePeriodQ16;
    uint32_t frequencyMilliHz;
    uint32_t periodCount;
} Frequency_Result;

/** 基于带滞回上升沿的流式频率估计器。 */
typedef struct {
    int16_t threshold;
    uint16_t hysteresis;
    uint32_t sampleRateHz;
    int16_t previousSample;
    uint64_t nextSampleIndex;
    uint64_t pendingCrossingQ16;
    uint64_t lastCrossingQ16;
    uint64_t periodSumQ16;
    uint32_t periodCount;
    bool initialized;
    bool lowArmed;
    bool pendingRising;
    bool haveLastCrossing;
} Frequency_Estimator;

/** 初始化并清空频率估计器。hysteresis 是阈值两侧的滞回宽度。 */
bool Frequency_init(Frequency_Estimator *estimator,
    uint32_t sampleRateHz, int16_t threshold, uint16_t hysteresis);

/** 清除采样历史和累计周期，但保留采样率、阈值及滞回配置。 */
void Frequency_reset(Frequency_Estimator *estimator);

/** 加入一个采样点。 */
bool Frequency_processSample(
    Frequency_Estimator *estimator, int16_t input);

/** 加入一块连续采样；sampleCount 可以为 0。 */
bool Frequency_processBlock(Frequency_Estimator *estimator,
    const int16_t *input, uint32_t sampleCount);

/** 取得平均周期和频率；少于一个完整周期时返回 false。 */
bool Frequency_getResult(
    const Frequency_Estimator *estimator, Frequency_Result *result);

/** 对单个数据块进行一次完整频率测量。 */
bool Frequency_calculate(const int16_t *input, uint32_t sampleCount,
    uint32_t sampleRateHz, int16_t threshold, uint16_t hysteresis,
    Frequency_Result *result);

#endif
