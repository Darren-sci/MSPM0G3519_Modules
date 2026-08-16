#ifndef ALGORITHMS_SPECTRAL_ANALYSIS_THD_THD_H_
#define ALGORITHMS_SPECTRAL_ANALYSIS_THD_THD_H_

#include <stdbool.h>
#include <stdint.h>

#include "Algorithms/spectral_analysis/harmonic_analysis/harmonic_analysis.h"

/** THD 计算结果，不包含噪声。 */
typedef struct {
    uint64_t harmonicPowerQ30;
    uint32_t fundamentalAmplitudeQ15;
    uint32_t harmonicRmsAmplitudeQ15;
    uint32_t thdQ15;
    uint32_t thdMilliPercent;
    uint16_t includedHarmonicCount;
    bool powerSaturated;
    bool valid;
} THD_Result;

/**
 * 根据 HarmonicAnalysis_analyze() 的结果计算 THD。
 *
 * components[0] 必须是有效基波。2次及以上所有 valid 分量的幅值平方
 * 求和，再开方并除以基波幅值。thdMilliPercent 的单位为 0.001%，例如
 * 1234 表示 1.234%。本函数计算 THD，不计算 THD+N。
 */
bool THD_calculate(const HarmonicAnalysis_Component *components,
    uint16_t componentCount, THD_Result *result);

#endif
