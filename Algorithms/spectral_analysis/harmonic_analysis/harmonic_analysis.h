#ifndef ALGORITHMS_SPECTRAL_ANALYSIS_HARMONIC_ANALYSIS_H_
#define ALGORITHMS_SPECTRAL_ANALYSIS_HARMONIC_ANALYSIS_H_

#include <stdbool.h>
#include <stdint.h>

#include "Algorithms/spectral_analysis/spectrum/spectrum.h"

/** 一次谐波搜索的结果；order=1 表示基波。 */
typedef struct {
    uint64_t expectedFrequencyMilliHz;
    uint64_t detectedFrequencyMilliHz;
    uint32_t amplitudeQ15;
    uint32_t peakPowerQ30;
    uint16_t order;
    uint16_t detectedBin;
    bool valid;
} HarmonicAnalysis_Component;

/** 根据奈奎斯特频率返回理论上可观察的最高谐波次数。 */
uint16_t HarmonicAnalysis_getMaximumOrder(
    const Spectrum_Config *config,
    uint64_t fundamentalFrequencyMilliHz);

/**
 * 分析1～maximumOrder次分量。
 *
 * 每次谐波在理论频点左右 searchRadiusBins 范围内寻找最大功率频点。
 * components 容量至少为 maximumOrder；超出奈奎斯特频率的分量会保留
 * order 和 expectedFrequencyMilliHz，但 valid 为 false。
 */
bool HarmonicAnalysis_analyze(const Spectrum_Config *config,
    const FFT_ComplexQ15 *spectrum,
    uint64_t fundamentalFrequencyMilliHz,
    uint16_t maximumOrder, uint16_t searchRadiusBins,
    HarmonicAnalysis_Component *components,
    uint16_t componentCapacity);

#endif
