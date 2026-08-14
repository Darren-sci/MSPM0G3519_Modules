#ifndef DRIVERS_ADC_MULTI_H_
#define DRIVERS_ADC_MULTI_H_

#include <stdbool.h>
#include <stdint.h>

/* ADC0 每个定时器事件依次采集四个物理通道。 */
#define ADC_MULTI_CHANNEL_COUNT        (4U)
#define ADC_MULTI_FRAME_COUNT          (512U)
#define ADC_MULTI_DEFAULT_RATE_HZ      (10000U)
#define ADC_MULTI_MIN_RATE_HZ          (1000U)
#define ADC_MULTI_MAX_RATE_HZ          (100000U)

/**
 * 四通道在 ADC 序列和 DMA 缓冲区中的固定顺序。
 * 如果以后改变模拟前端用途，只需统一修改这里和使用文档。
 */
typedef enum {
    ADC_MULTI_VIN = 0,
    ADC_MULTI_IIN,
    ADC_MULTI_VOUT,
    ADC_MULTI_IOUT
} ADCMulti_Channel;

/**
 * 一次定时器触发产生一帧。四个成员按 MEM0～MEM3 的顺序排列，
 * 与 ADC FIFO 输出顺序一致。
 */
typedef struct {
    uint16_t vin;
    uint16_t iin;
    uint16_t vout;
    uint16_t iout;
} ADCMulti_Frame;

/** 在 SYSCFG_DL_init() 之后初始化驱动，但暂不开始采样。 */
void ADCMulti_init(void);

/**
 * 以指定的每通道采样率启动连续采样。
 *
 * @param frameRateHz 每秒采样帧数；一帧包含四次 ADC 转换。
 * @return 参数和定时器装载值有效时返回 true。
 */
bool ADCMulti_start(uint32_t frameRateHz);

/** 停止采样定时器、ADC DMA 和 DMA 通道。 */
void ADCMulti_stop(void);

/**
 * 取得一块已经采满的只读缓冲区。
 * 使用完成后必须调用 ADCMulti_releaseBuffer()，否则驱动不会覆盖该缓冲区。
 */
bool ADCMulti_getReadyBuffer(
    const ADCMulti_Frame **frames, uint16_t *frameCount);

/** 释放由 ADCMulti_getReadyBuffer() 返回的缓冲区。 */
void ADCMulti_releaseBuffer(const ADCMulti_Frame *frames);

/** 返回定时器整除后得到的实际每通道采样率。 */
uint32_t ADCMulti_getActualFrameRate(void);

/** 返回因 CPU 未及时释放缓冲区而丢弃的数据块数量。 */
uint32_t ADCMulti_getOverrunCount(void);

/** 返回驱动当前是否正在采样。 */
bool ADCMulti_isRunning(void);

#endif
