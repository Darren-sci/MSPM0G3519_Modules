#ifndef ALGORITHMS_POWER_ANALYSIS_RIPPLE_RIPPLE_H_
#define ALGORITHMS_POWER_ANALYSIS_RIPPLE_RIPPLE_H_

#include <stdbool.h>
#include <stdint.h>

/** 一块电源输出数据的直流与纹波指标。 */
typedef struct {
    int32_t dcMean;
    int32_t minimum;
    int32_t maximum;
    uint32_t totalRms;
    uint32_t rippleRms;
    uint32_t ripplePeakToPeak;
    uint32_t positivePeakDeviation;
    uint32_t negativePeakDeviation;
    uint32_t rippleFactorQ15;
    uint32_t rippleRmsMilliPercent;
    uint32_t ripplePeakToPeakMilliPercent;
    uint32_t sampleCount;
    bool percentageValid;
    bool calculationOverflow;
    bool valid;
} Ripple_Result;

/**
 * 计算平均直流、总RMS、交流纹波RMS、峰峰值和纹波百分比。
 * 输入可使用 ADC 校准后的任意一致整数单位，例如 mV。sampleCount 必须
 * 大于0。
 */
bool Ripple_calculate(const int32_t *samples,
    uint32_t sampleCount, Ripple_Result *result);

#endif
