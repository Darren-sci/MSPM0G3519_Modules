#ifndef ALGORITHMS_FILTERS_FIR_FIR_H_
#define ALGORITHMS_FILTERS_FIR_FIR_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * Q15 FIR 滤波器实例。
 *
 * coefficients 和 state 所指向的存储区由调用者提供，并且在滤波器使用
 * 期间必须始终有效。多个实例可以共享只读系数，但不能共享状态缓冲区。
 */
typedef struct {
    const int16_t *coefficients;
    int16_t *state;
    uint16_t tapCount;
    uint16_t writeIndex;
} FIR_Filter;

/**
 * 初始化一个 Q15 FIR 滤波器并清零状态。
 *
 * coefficients[0] 对应当前输入，coefficients[n] 对应延迟 n 个采样点的
 * 输入。state 至少需要容纳 tapCount 个 int16_t。
 */
bool FIR_init(FIR_Filter *filter, const int16_t *coefficients,
    int16_t *state, uint16_t tapCount);

/** 清除历史输入，但保留系数和抽头数量。 */
void FIR_reset(FIR_Filter *filter);

/**
 * 处理一个 Q15 采样点。
 *
 * 返回值经过 Q15 四舍五入并限制在 INT16_MIN～INT16_MAX。filter 必须已经
 * 由 FIR_init() 成功初始化。
 */
int16_t FIR_processSample(FIR_Filter *filter, int16_t input);

/**
 * 处理一块连续的 Q15 数据。
 *
 * input 与 output 可以指向同一缓冲区。sampleCount 可以为 0。
 */
bool FIR_processBlock(FIR_Filter *filter, const int16_t *input,
    int16_t *output, uint32_t sampleCount);

#endif
