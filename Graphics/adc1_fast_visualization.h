#ifndef GRAPHICS_ADC1_FAST_VISUALIZATION_H_
#define GRAPHICS_ADC1_FAST_VISUALIZATION_H_

#include <stdint.h>

#include "Algorithms/analysis_pipeline/signal_analyzer.h"

/** 绘制ADC1高速采集的时域波形、单边频谱和主要测量参数。 */
void ADC1FastVisualization_draw(
    const uint16_t *samples,
    uint16_t sampleCount,
    uint32_t sampleRateHz,
    const SignalAnalyzer *analyzer,
    const SignalAnalyzer_Result *result,
    uint32_t overrunCount);

#endif
