#ifndef GRAPHICS_ADC1_FAST_VISUALIZATION_H_
#define GRAPHICS_ADC1_FAST_VISUALIZATION_H_

#include <stdint.h>

#include "Algorithms/analysis_pipeline/signal_analyzer.h"

/** 全屏示波器页面：ADC中点固定在屏幕中央，并对周期信号进行上升沿对齐。 */
void ADC1FastVisualization_drawScope(
    const uint16_t *samples,
    uint16_t sampleCount,
    uint32_t sampleRateHz,
    const SignalAnalyzer_Result *result,
    uint32_t overrunCount);

/** 单边频谱页面：左侧为0～Fs/2谱图，右侧显示H1～H6频率和Vpk。 */
void ADC1FastVisualization_drawSpectrum(
    const SignalAnalyzer *analyzer,
    uint32_t sampleRateHz,
    const SignalAnalyzer_Result *result,
    uint32_t overrunCount);

#endif
