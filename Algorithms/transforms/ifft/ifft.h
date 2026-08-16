#ifndef ALGORITHMS_TRANSFORMS_IFFT_IFFT_H_
#define ALGORITHMS_TRANSFORMS_IFFT_IFFT_H_

#include <stdbool.h>
#include <stdint.h>

#include "Algorithms/transforms/fft/fft.h"

/** IFFT 内部宽位工作元素。调用者只负责分配，不应直接解释其中数据。 */
typedef struct {
    int32_t real;
    int32_t imag;
} IFFT_ComplexI32;

/** 每次 IFFT 执行返回的级数和饱和信息。 */
typedef struct {
    uint8_t stageCount;
    uint32_t internalSaturationCount;
    uint32_t outputSaturationCount;
} IFFT_ExecutionInfo;

/**
 * 返回 IFFT 所需的 IFFT_ComplexI32 工作元素数量。
 * 无效或未初始化的 FFT 计划返回 0。
 */
uint16_t IFFT_getRequiredWorkspaceCount(const FFT_PlanQ15 *fftPlan);

/**
 * 对本项目 FFT_execute() 产生的归一化复数频谱执行逆变换。
 *
 * fftPlan 必须是生成该频谱时使用的同长度 FFT 计划。IFFT 直接复用其中
 * 的旋转因子，不需要额外的 IFFT 初始化和旋转因子数组。
 *
 * spectrum 和 output 均至少包含 fftPlan->length 个 FFT_ComplexQ15；二者
 * 可以相同，从而原地覆盖频谱。workspace 至少包含
 * IFFT_getRequiredWorkspaceCount() 个元素，且不能与 spectrum 或 output
 * 重叠。
 *
 * 当前 FFT 每一级固定除以 2，频谱已经等于 DFT/N。因此这里不再进行
 * 逐级缩放，最终 output 可直接恢复原时域复数数据。若传入未经本项目
 * FFT 归一化的普通 DFT 结果，输出会额外放大 N 倍并可能饱和。
 */
bool IFFT_execute(const FFT_PlanQ15 *fftPlan,
    const FFT_ComplexQ15 *spectrum,
    IFFT_ComplexI32 *workspace,
    FFT_ComplexQ15 *output,
    IFFT_ExecutionInfo *info);

#endif
