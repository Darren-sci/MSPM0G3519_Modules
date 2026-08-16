#ifndef ALGORITHMS_SPECTRAL_ANALYSIS_FUNDAMENTAL_DETECTION_H_
#define ALGORITHMS_SPECTRAL_ANALYSIS_FUNDAMENTAL_DETECTION_H_

#include <stdbool.h>
#include <stdint.h>

#include "Algorithms/spectral_analysis/spectrum/spectrum.h"

/** 基波搜索结果。binOffsetQ15 范围为约 -0.5～+0.5 个频点。 */
typedef struct {
    uint64_t frequencyMilliHz;
    uint32_t amplitudeQ15;
    uint32_t peakPowerQ30;
    uint16_t peakBin;
    int16_t binOffsetQ15;
    bool atSearchBoundary;
    bool valid;
} FundamentalDetection_Result;

/**
 * 在闭区间 [firstBin, lastBin] 内寻找最强频点，并用左右相邻功率进行
 * 抛物线插值。通常 firstBin 应从 1 开始，以排除直流。
 */
bool FundamentalDetection_find(const Spectrum_Config *config,
    const FFT_ComplexQ15 *spectrum,
    uint16_t firstBin, uint16_t lastBin,
    FundamentalDetection_Result *result);

#endif
