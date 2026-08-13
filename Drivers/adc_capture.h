#ifndef DRIVERS_ADC_CAPTURE_H_
#define DRIVERS_ADC_CAPTURE_H_

#include <stdbool.h>
#include <stdint.h>

#define ADC_CAPTURE_SAMPLE_COUNT    (4096U)

/**
 * 初始化ADC采集驱动。
 *
 * SysConfig已经完成ADC和DMA外设的基本初始化；
 * 此函数负责设置DMA源地址为ADC MEM0结果寄存器。
 */
void ADCCapture_init(void);

/**
 * 启动一次ADC采集。
 *
 * DMA采满ADC_CAPTURE_SAMPLE_COUNT个采样值后返回。
 *
 * @param samples 返回采样数组首地址。
 * @return true表示采集完成，false表示超时。
 */
bool ADCCapture_acquire(const uint16_t **samples);

#endif