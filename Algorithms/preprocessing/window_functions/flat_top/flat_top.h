#ifndef ALGORITHMS_PREPROCESSING_WINDOW_FUNCTIONS_FLAT_TOP_FLAT_TOP_H_
#define ALGORITHMS_PREPROCESSING_WINDOW_FUNCTIONS_FLAT_TOP_FLAT_TOP_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * 五项 Flat-top 窗实例。
 *
 * coefficients 由调用者提供，至少包含 length 个 int16_t。系数、输入和
 * 输出均使用 Q15；powerGainQ30 是系数平方平均值的 Q30 表示。
 */
typedef struct {
    int16_t *coefficients;
    uint16_t length;
    uint16_t coherentGainQ15;
    uint32_t powerGainQ30;
} FlatTop_Window;

/**
 * 生成指定长度的对称五项 Flat-top 窗并计算增益参数。
 *
 * length 必须不小于 5。初始化使用纯定点 CORDIC，不依赖浮点或数学库。
 */
bool FlatTop_init(FlatTop_Window *window,
    int16_t *coefficients, uint16_t length);

/**
 * 对一块与窗口等长的 Q15 数据加窗。
 *
 * input 与 output 可以指向同一缓冲区。
 */
bool FlatTop_apply(const FlatTop_Window *window,
    const int16_t *input, int16_t *output);

/** 返回窗口长度；无效指针返回 0。 */
uint16_t FlatTop_getLength(const FlatTop_Window *window);

/** 返回相干增益的 Q15 表示；无效指针返回 0。 */
uint16_t FlatTop_getCoherentGainQ15(const FlatTop_Window *window);

/** 返回功率增益的 Q30 表示；无效指针返回 0。 */
uint32_t FlatTop_getPowerGainQ30(const FlatTop_Window *window);

#endif
