#ifndef GRAPHICS_ADC_MULTI_VISUALIZATION_H_
#define GRAPHICS_ADC_MULTI_VISUALIZATION_H_

#include <stdint.h>

#include "Drivers/adc_multi.h"

/**
 * 统计并显示一块四通道 ADC 数据，供接线和通道顺序检查使用。
 * 页面显示每路平均码值、按 3.3 V 参考换算的电压、实际帧率和溢出次数。
 */
void ADCMultiVisualization_draw(
    const ADCMulti_Frame *frames,
    uint16_t frameCount,
    uint32_t frameRateHz,
    uint32_t overrunCount);

/** 显示四通道 ADC 无法启动或暂无数据。 */
void ADCMultiVisualization_drawError(const char *message);

#endif
