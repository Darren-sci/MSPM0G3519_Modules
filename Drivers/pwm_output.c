#include "Drivers/pwm_output.h"

#include "ti_msp_dl_config.h"

/*
 * 当前 PWM 使用边沿对齐向下计数模式：
 * 比较值等于周期值时为 0%，比较值等于 0 时为 100%。
 * 周期值直接从定时器读取，因此以后在 SysConfig 中修改频率后无需改本文件。
 */
static uint32_t PWMOutput_dutyToCompare(uint16_t dutyPermille)
{
    uint32_t period = DL_TimerA_getLoadValue(PWM_0_INST);

    if (dutyPermille > PWM_OUTPUT_DUTY_MAX) {
        dutyPermille = PWM_OUTPUT_DUTY_MAX;
    }

    return period -
        ((period * (uint32_t)dutyPermille) / PWM_OUTPUT_DUTY_MAX);
}

void PWMOutput_setChannel0Duty(uint16_t dutyPermille)
{
    DL_TimerA_setCaptureCompareValue(
        PWM_0_INST,
        PWMOutput_dutyToCompare(dutyPermille),
        GPIO_PWM_0_C0_IDX);
}

void PWMOutput_setChannel1Duty(uint16_t dutyPermille)
{
    DL_TimerA_setCaptureCompareValue(
        PWM_0_INST,
        PWMOutput_dutyToCompare(dutyPermille),
        GPIO_PWM_0_C1_IDX);
}
