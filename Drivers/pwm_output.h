#ifndef DRIVERS_PWM_OUTPUT_H_
#define DRIVERS_PWM_OUTPUT_H_

#include <stdint.h>

/* 占空比采用千分比表示：0 表示 0%，500 表示 50%，1000 表示 100%。 */
#define PWM_OUTPUT_DUTY_MAX    (1000U)

/**
 * 设置 PWM Channel 0（当前由 SysConfig 映射到 PA0）的占空比。
 *
 * @param dutyPermille 占空比千分比；超过 1000 时自动限制为 1000。
 */
void PWMOutput_setChannel0Duty(uint16_t dutyPermille);

/**
 * 设置 PWM Channel 1（当前由 SysConfig 映射到 PA1）的占空比。
 *
 * @param dutyPermille 占空比千分比；超过 1000 时自动限制为 1000。
 */
void PWMOutput_setChannel1Duty(uint16_t dutyPermille);

#endif
