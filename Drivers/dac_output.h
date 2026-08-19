#ifndef DRIVERS_DAC_OUTPUT_H_
#define DRIVERS_DAC_OUTPUT_H_

#include <stdbool.h>
#include <stdint.h>

#define DAC_OUTPUT_MAX_CODE             (4095U)
#define DAC_OUTPUT_MAX_TABLE_LENGTH     (4096U)

/** DAC内部采样发生器支持的离散更新率。 */
typedef enum {
    DAC_OUTPUT_RATE_500_HZ = 500U,
    DAC_OUTPUT_RATE_1_KHZ = 1000U,
    DAC_OUTPUT_RATE_2_KHZ = 2000U,
    DAC_OUTPUT_RATE_4_KHZ = 4000U,
    DAC_OUTPUT_RATE_8_KHZ = 8000U,
    DAC_OUTPUT_RATE_16_KHZ = 16000U,
    DAC_OUTPUT_RATE_100_KHZ = 100000U,
    DAC_OUTPUT_RATE_200_KHZ = 200000U,
    DAC_OUTPUT_RATE_500_KHZ = 500000U,
    DAC_OUTPUT_RATE_1_MHZ = 1000000U
} DACOutput_SampleRate;

/**
 * 在 SYSCFG_DL_init() 之后初始化DAC驱动并输出0码。
 * 初始化会停止SysConfig预配置的采样发生器和DMA，DAC不会自行输出波形。
 */
void DACOutput_init(void);

/** 停止波形DMA并直接输出一个12位DAC码。 */
bool DACOutput_setCode(uint16_t code);

/**
 * 根据参考电压设置直流输出。
 * referenceMilliVolts应填写实测VDDA；目标电压不能高于参考电压。
 */
bool DACOutput_setMilliVolts(
    uint32_t outputMilliVolts, uint32_t referenceMilliVolts);

/**
 * 使用DMA_CH2和DAC FIFO循环输出波形表。
 * table在停止波形前必须始终有效，表内每个元素均应位于0～4095。
 */
bool DACOutput_startWaveform(const uint16_t *table,
    uint16_t tableLength, DACOutput_SampleRate sampleRate);

/** 停止DMA波形输出并保持最近的DAC电平。 */
void DACOutput_stopWaveform(void);

/** 返回当前波形的理论重复频率，单位mHz；未运行时返回0。 */
uint64_t DACOutput_getWaveformFrequencyMilliHz(void);

/** 返回DAC FIFO欠载中断累计次数。 */
uint32_t DACOutput_getUnderrunCount(void);

/** 返回DMA循环波形当前是否正在运行。 */
bool DACOutput_isWaveformRunning(void);

#endif
