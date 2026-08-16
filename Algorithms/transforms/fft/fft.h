#ifndef ALGORITHMS_TRANSFORMS_FFT_FFT_H_
#define ALGORITHMS_TRANSFORMS_FFT_FFT_H_

#include <stdbool.h>
#include <stdint.h>

#define FFT_MIN_LENGTH    (16U)
#define FFT_MAX_LENGTH    (1024U)

/** Q15 复数，实部和虚部分别占一个 int16_t。 */
typedef struct {
    int16_t real;
    int16_t imag;
} FFT_ComplexQ15;

/**
 * Q15 FFT 计划。
 *
 * twiddles 指向调用者提供的只读旋转因子缓冲区。一个计划可以依次用于
 * 多个通道，但同一个数据工作区不能同时执行多个 FFT。
 */
typedef struct {
    const FFT_ComplexQ15 *twiddles;
    uint16_t length;
    uint16_t twiddleCount;
    uint8_t stageCount;
    bool initialized;
} FFT_PlanQ15;

/** 每次 FFT 执行返回的定点缩放和饱和信息。 */
typedef struct {
    uint8_t stageCount;
    uint8_t scaleShift;
    uint32_t saturationCount;
} FFT_ExecutionInfo;

/** 判断长度是否为模块支持的 2 次幂。 */
bool FFT_isLengthSupported(uint16_t length);

/** 返回指定长度所需的 FFT_ComplexQ15 旋转因子数量；无效长度返回 0。 */
uint16_t FFT_getRequiredTwiddleCount(uint16_t length);

/**
 * 初始化 Q15 FFT 计划并生成旋转因子。
 *
 * twiddleBuffer 至少需要 FFT_getRequiredTwiddleCount(length) 个元素。
 * 初始化不使用动态内存和浮点运算，只应在长度改变时重新执行。
 */
bool FFT_init(FFT_PlanQ15 *plan, uint16_t length,
    FFT_ComplexQ15 *twiddleBuffer, uint16_t twiddleCapacity);

/** 返回 FFT 长度；无效或未初始化计划返回 0。 */
uint16_t FFT_getLength(const FFT_PlanQ15 *plan);

/** 返回实数输入的非负频率频点数，即 length/2+1。 */
uint16_t FFT_getRealBinCount(const FFT_PlanQ15 *plan);

/**
 * 把与计划等长的实数 Q15 数据装入复数工作区，虚部清零。
 *
 * input 与 output 不能重叠；output 至少包含 plan->length 个复数元素。
 */
bool FFT_loadReal(const FFT_PlanQ15 *plan,
    const int16_t *input, FFT_ComplexQ15 *output);

/**
 * 对复数工作区执行原地、正向、基 2 Q15 FFT。
 *
 * 每一级固定缩小 1 位，因此最终复数谱等于数学 DFT 除以 FFT 长度。
 * info 可为空；非零 saturationCount 表示输入余量不足，结果可能失真。
 */
bool FFT_execute(const FFT_PlanQ15 *plan,
    FFT_ComplexQ15 *data, FFT_ExecutionInfo *info);

/** 先装载实数输入，再执行 FFT 的便捷接口。 */
bool FFT_executeReal(const FFT_PlanQ15 *plan, const int16_t *input,
    FFT_ComplexQ15 *output, FFT_ExecutionInfo *info);

#endif
