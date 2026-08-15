#ifndef ALGORITHMS_MEASUREMENTS_PEAK_TO_PEAK_PEAK_TO_PEAK_H_
#define ALGORITHMS_MEASUREMENTS_PEAK_TO_PEAK_PEAK_TO_PEAK_H_

#include <stdbool.h>
#include <stdint.h>

/** 可跨多个数据块使用的最小值、最大值和峰峰值累加器。 */
typedef struct {
    int32_t minimum;
    int32_t maximum;
    uint32_t sampleCount;
} PeakToPeak_Accumulator;

/** 峰峰值计算结果。span 始终非负，可覆盖完整 int32_t 输入范围。 */
typedef struct {
    int32_t minimum;
    int32_t maximum;
    uint32_t span;
} PeakToPeak_Result;

/** 清空累加器。 */
void PeakToPeak_reset(PeakToPeak_Accumulator *accumulator);

/** 加入一个 int32_t 样本；样本计数已满时返回 false。 */
bool PeakToPeak_addSample(
    PeakToPeak_Accumulator *accumulator, int32_t sample);

/** 加入一块 int16_t 数据；sampleCount 可以为 0。 */
bool PeakToPeak_addBlockInt16(PeakToPeak_Accumulator *accumulator,
    const int16_t *input, uint32_t sampleCount);

/** 加入一块 int32_t 数据；sampleCount 可以为 0。 */
bool PeakToPeak_addBlockInt32(PeakToPeak_Accumulator *accumulator,
    const int32_t *input, uint32_t sampleCount);

/** 取得最小值、最大值和峰峰值；没有样本时返回 false。 */
bool PeakToPeak_get(const PeakToPeak_Accumulator *accumulator,
    PeakToPeak_Result *result);

/** 返回累加器当前包含的样本数量。 */
uint32_t PeakToPeak_getSampleCount(
    const PeakToPeak_Accumulator *accumulator);

/** 一次性计算一块 int16_t 数据。 */
bool PeakToPeak_calculateInt16(const int16_t *input,
    uint32_t sampleCount, PeakToPeak_Result *result);

/** 一次性计算一块 int32_t 数据。 */
bool PeakToPeak_calculateInt32(const int32_t *input,
    uint32_t sampleCount, PeakToPeak_Result *result);

#endif
