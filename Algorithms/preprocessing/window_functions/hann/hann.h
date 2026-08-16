#ifndef ALGORITHMS_PREPROCESSING_WINDOW_FUNCTIONS_HANN_HANN_H_
#define ALGORITHMS_PREPROCESSING_WINDOW_FUNCTIONS_HANN_HANN_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * Hann 窗实例。
 *
 * coefficients 由调用者提供，至少包含 length 个 int16_t。系数、输入和
 * 输出均使用 Q15；powerGainQ30 是系数平方平均值的 Q30 表示。
 */
typedef struct {
    int16_t *coefficients;
    uint16_t length;
    uint16_t coherentGainQ15;
    uint32_t powerGainQ30;
} Hann_Window;

/**
 * 生成指定长度的对称 Hann 窗并计算增益参数。
 *
 * length 必须不小于 3。初始化完成后，coefficients 在窗口使用期间必须
 * 保持有效。函数不使用动态内存和浮点运算。
 */
bool Hann_init(Hann_Window *window,
    int16_t *coefficients, uint16_t length);

/**
 * 对一块与窗口等长的 Q15 数据加窗。
 *
 * input 与 output 可以指向同一缓冲区。
 */
bool Hann_apply(const Hann_Window *window,
    const int16_t *input, int16_t *output);

/** 返回窗口长度；无效指针返回 0。 */
uint16_t Hann_getLength(const Hann_Window *window);

/** 返回相干增益的 Q15 表示；无效指针返回 0。 */
uint16_t Hann_getCoherentGainQ15(const Hann_Window *window);

/** 返回功率增益的 Q30 表示；无效指针返回 0。 */
uint32_t Hann_getPowerGainQ30(const Hann_Window *window);

#endif
