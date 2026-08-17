#ifndef ALGORITHMS_DETECTION_OVERRANGE_DETECTION_H_
#define ALGORITHMS_DETECTION_OVERRANGE_DETECTION_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int32_t lowerLimit;
    int32_t upperLimit;
    uint32_t releaseHysteresis;
    uint32_t consecutiveAssertSamples;
    uint32_t consecutiveReleaseSamples;
} OverrangeDetection_Config;

typedef struct {
    OverrangeDetection_Config config;
    uint32_t lowRunLength;
    uint32_t highRunLength;
    uint32_t lowReleaseRunLength;
    uint32_t highReleaseRunLength;
    uint64_t totalSampleCount;
    bool lowActive;
    bool highActive;
    bool initialized;
} OverrangeDetection_State;

/** 一块数据内的超量程统计和处理结束后的状态。 */
typedef struct {
    int32_t minimum;
    int32_t maximum;
    uint32_t lowSampleCount;
    uint32_t highSampleCount;
    uint32_t longestLowRun;
    uint32_t longestHighRun;
    uint32_t firstLowIndex;
    uint32_t firstHighIndex;
    bool lowAssertedThisBlock;
    bool highAssertedThisBlock;
    bool lowReleasedThisBlock;
    bool highReleasedThisBlock;
    bool lowActive;
    bool highActive;
    bool valid;
} OverrangeDetection_Result;

/** 初始化带连续点确认和释放迟滞的超量程检测器。 */
bool OverrangeDetection_init(OverrangeDetection_State *state,
    const OverrangeDetection_Config *config);

/** 清除运行历史，但保留配置。 */
bool OverrangeDetection_reset(OverrangeDetection_State *state);

/**
 * 处理一块有符号校准数据。状态会跨数据块保留，因此能检测跨块连续削顶。
 */
bool OverrangeDetection_processBlock(
    OverrangeDetection_State *state,
    const int32_t *samples, uint32_t sampleCount,
    OverrangeDetection_Result *result);

#endif
