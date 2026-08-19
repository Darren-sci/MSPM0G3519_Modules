#ifndef DRIVERS_ADC1_FAST_H_
#define DRIVERS_ADC1_FAST_H_

#include <stdbool.h>
#include <stdint.h>

/*
 * 每块1024点可直接交给1024点FFT；两个缓冲区合计占用4096字节SRAM。
 * FIFO每次输出两个16位结果，因此采样点数量必须是偶数。
 */
#define ADC1_FAST_SAMPLE_COUNT    (1024U)

/**
 * 在 SYSCFG_DL_init() 之后初始化ADC1高速采集驱动。
 * 本函数只准备驱动状态和中断，不会开始转换。
 */
void ADC1Fast_init(void);

/**
 * 启动ADC1单通道连续高速采集。
 * ADC由软件启动一次后自行连续转换，不使用任何定时器。
 */
bool ADC1Fast_start(void);

/** 停止ADC、DMA和块完成中断产生的数据流。 */
void ADC1Fast_stop(void);

/**
 * 非阻塞取得一块已经采满的数据。
 * 使用完成后必须调用 ADC1Fast_releaseBuffer()。
 */
bool ADC1Fast_getReadyBuffer(
    const uint16_t **samples, uint16_t *sampleCount);

/** 释放由 ADC1Fast_getReadyBuffer() 返回的缓冲区。 */
void ADC1Fast_releaseBuffer(const uint16_t *samples);

/** 返回因CPU未及时领取或释放缓冲区而丢弃的数据块数量。 */
uint32_t ADC1Fast_getOverrunCount(void);

/** 返回ADC1当前是否正在连续采集。 */
bool ADC1Fast_isRunning(void);

#endif
