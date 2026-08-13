#ifndef DRIVERS_SPWM_H_
#define DRIVERS_SPWM_H_

#include <stdint.h>

/* 本模块使用 TIMA0 的两个通道，通道号只能取 0 或 1。 */
#define SPWM_CHANNEL_0              (0U)
#define SPWM_CHANNEL_1              (1U)
#define SPWM_CHANNEL_COUNT          (2U)

/* 幅度采用千分比：0 表示无交流分量，1000 表示最大调制度。 */
#define SPWM_AMPLITUDE_MAX          (1000U)

/* SysConfig 中固定的 PWM 载波频率，同时也是正弦占空比更新频率。 */
#define SPWM_UPDATE_FREQUENCY_HZ    (20000U)

/**
 * 初始化双通道 SPWM 驱动。
 *
 * 调用前必须先调用 SYSCFG_DL_init()。初始化后两路均停止，输出保持
 * 50% 占空比；经过隔直或运放去偏置后，交流分量为 0。
 */
void SPWM_init(void);

/** 启动指定通道；启动时从该通道设置的相位开始。 */
void SPWM_start(uint8_t channel);

/** 停止指定通道，并使其回到 50% 占空比。 */
void SPWM_stop(uint8_t channel);

/** 停止全部通道，使两路都回到 50% 占空比。 */
void SPWM_stopAll(void);

/**
 * 设置滤波后正弦波的频率，单位 Hz。
 * 可设置范围为 0～9999 Hz；超出范围会限制为 9999 Hz。
 */
void SPWM_setFrequency(uint8_t channel, uint32_t frequencyHz);

/**
 * 设置正弦波幅度（调制度），范围 0～1000。
 * 0 表示无交流分量，1000 表示 PWM 占空比理论范围为 0～100%。
 */
void SPWM_setAmplitude(uint8_t channel, uint16_t amplitudePermille);

/** 设置起始相位，单位为度；大于等于 360 的值会自动取余。 */
void SPWM_setPhase(uint8_t channel, uint16_t phaseDegree);

/** 一次性设置指定通道的频率、幅度和相位。 */
void SPWM_set(uint8_t channel, uint32_t frequencyHz,
    uint16_t amplitudePermille, uint16_t phaseDegree);

#endif
