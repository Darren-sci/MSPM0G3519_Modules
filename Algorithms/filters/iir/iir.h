#ifndef ALGORITHMS_FILTERS_IIR_IIR_H_
#define ALGORITHMS_FILTERS_IIR_IIR_H_

#include <stdbool.h>
#include <stdint.h>

/* Q30 中的 1.0，供定义滤波器系数时使用。 */
#define IIR_Q30_ONE    (1073741824L)

/**
 * 一个二阶 IIR 节的 Q30 系数。
 *
 * 系数采用常见的分母符号约定：输出计算时减去 a1*y[n-1] 和
 * a2*y[n-2]。设计工具给出分母 1 + a1*z^-1 + a2*z^-2 时，a1、a2
 * 可以直接使用，不要再次改变符号。
 */
typedef struct {
    int32_t b0;
    int32_t b1;
    int32_t b2;
    int32_t a1;
    int32_t a2;
} IIR_BiquadCoefficients;

/** 一个二阶节的 Q15 输入和输出历史。 */
typedef struct {
    int16_t x1;
    int16_t x2;
    int16_t y1;
    int16_t y2;
} IIR_BiquadState;

/** 由一个或多个二阶节级联组成的 IIR 滤波器实例。 */
typedef struct {
    const IIR_BiquadCoefficients *coefficients;
    IIR_BiquadState *state;
    uint16_t sectionCount;
} IIR_Filter;

/**
 * 初始化 IIR 滤波器并清除全部历史状态。
 *
 * coefficients 和 state 都必须至少包含 sectionCount 个元素，并在滤波器
 * 使用期间保持有效。函数不检查系数所代表的滤波器是否稳定。
 */
bool IIR_init(IIR_Filter *filter,
    const IIR_BiquadCoefficients *coefficients,
    IIR_BiquadState *state, uint16_t sectionCount);

/** 清除所有二阶节的输入和输出历史，但保留系数配置。 */
void IIR_reset(IIR_Filter *filter);

/**
 * 处理一个 Q15 采样点。
 *
 * 数据依次通过所有二阶节，每节输出都会四舍五入并饱和到 Q15。filter
 * 必须已经由 IIR_init() 成功初始化。
 */
int16_t IIR_processSample(IIR_Filter *filter, int16_t input);

/**
 * 处理一块连续的 Q15 数据。
 *
 * input 与 output 可以指向同一缓冲区。sampleCount 可以为 0。
 */
bool IIR_processBlock(IIR_Filter *filter, const int16_t *input,
    int16_t *output, uint32_t sampleCount);

#endif
