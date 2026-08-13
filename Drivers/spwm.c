#include "Drivers/spwm.h"

#include <stdbool.h>

#include "ti_msp_dl_config.h"

/*
 * 这里使用 32 位相位累加器：一整圈 0～360° 对应 0～2^32。
 * 每次 PWM 周期中断都增加 phaseStep，因此无需改变 SysConfig 就能改频率。
 */
typedef struct {
    volatile uint32_t phaseAccumulator;
    volatile uint32_t phaseStart;
    volatile uint32_t phaseStep;
    volatile uint16_t amplitudePermille;
    volatile bool enabled;
} SPWM_ChannelState;

static SPWM_ChannelState gSPWMChannels[SPWM_CHANNEL_COUNT];
static volatile bool gSPWMTimerRunning;

/*
 * 0～90°的四分之一正弦表，数值范围 0～32767。
 * 利用正弦波的对称性即可还原整周期，既节省 Flash，又避免中断中计算浮点数。
 */
static const uint16_t gSPWMSineQuarter[65] = {
       0,   804,  1608,  2410,  3212,  4011,  4808,  5602,
    6393,  7179,  7962,  8739,  9512, 10278, 11039, 11793,
   12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530,
   18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
   23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790,
   27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
   30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971,
   32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
   32767
};

static bool SPWM_isChannelValid(uint8_t channel)
{
    return channel < SPWM_CHANNEL_COUNT;
}

/* 根据相位累加器最高 8 位，取得一个完整周期的有符号正弦值。 */
static int16_t SPWM_sineFromPhase(uint32_t phase)
{
    uint8_t index = (uint8_t) (phase >> 24);
    uint8_t quadrant = index >> 6;
    uint8_t offset = index & 0x3FU;
    uint16_t magnitude;

    if ((quadrant == 1U) || (quadrant == 3U)) {
        offset = (uint8_t) (64U - offset);
    }
    magnitude = gSPWMSineQuarter[offset];

    if (quadrant >= 2U) {
        return (int16_t) -(int32_t) magnitude;
    }
    return (int16_t) magnitude;
}

/* 将正弦值和幅度换算为中心在 50% 的占空比，再换成硬件比较值。 */
static uint32_t SPWM_phaseToCompare(
    uint32_t phase, uint16_t amplitudePermille)
{
    uint32_t loadValue = DL_TimerA_getLoadValue(PWM_0_INST);
    int32_t sine = SPWM_sineFromPhase(phase);
    /* 先换算成 -500～+500，整个乘法过程不会超过 int32_t。 */
    int32_t dutyPermille = 500 +
        (sine * (int32_t) amplitudePermille) / (32767 * 2);

    if (dutyPermille < 0) {
        dutyPermille = 0;
    } else if (dutyPermille > (int32_t) SPWM_AMPLITUDE_MAX) {
        dutyPermille = SPWM_AMPLITUDE_MAX;
    }

    return loadValue -
        ((loadValue * (uint32_t) dutyPermille) / SPWM_AMPLITUDE_MAX);
}

static void SPWM_writeMidpoint(uint8_t channel)
{
    uint32_t loadValue = DL_TimerA_getLoadValue(PWM_0_INST);
    uint32_t ccIndex = (channel == SPWM_CHANNEL_0) ?
        GPIO_PWM_0_C0_IDX : GPIO_PWM_0_C1_IDX;

    DL_TimerA_setCaptureCompareValue(PWM_0_INST, loadValue / 2U, ccIndex);
}

static void SPWM_updateChannel(uint8_t channel)
{
    SPWM_ChannelState *state = &gSPWMChannels[channel];
    uint32_t ccIndex = (channel == SPWM_CHANNEL_0) ?
        GPIO_PWM_0_C0_IDX : GPIO_PWM_0_C1_IDX;

    if (!state->enabled) {
        return;
    }

    DL_TimerA_setCaptureCompareValue(PWM_0_INST,
        SPWM_phaseToCompare(
            state->phaseAccumulator, state->amplitudePermille),
        ccIndex);
    state->phaseAccumulator += state->phaseStep;
}

void SPWM_init(void)
{
    uint8_t channel;

    DL_TimerA_stopCounter(PWM_0_INST);
    NVIC_DisableIRQ(PWM_0_INST_INT_IRQN);
    NVIC_ClearPendingIRQ(PWM_0_INST_INT_IRQN);

    for (channel = 0U; channel < SPWM_CHANNEL_COUNT; channel++) {
        gSPWMChannels[channel].phaseAccumulator = 0U;
        gSPWMChannels[channel].phaseStart = 0U;
        gSPWMChannels[channel].phaseStep = 0U;
        gSPWMChannels[channel].amplitudePermille = 0U;
        gSPWMChannels[channel].enabled = false;
        SPWM_writeMidpoint(channel);
    }
    gSPWMTimerRunning = false;

    /* SysConfig 已开启 TIMA0 的 ZERO 中断，此处只打开 NVIC。 */
    NVIC_EnableIRQ(PWM_0_INST_INT_IRQN);
}

void SPWM_start(uint8_t channel)
{
    if (!SPWM_isChannelValid(channel)) {
        return;
    }

    gSPWMChannels[channel].phaseAccumulator =
        gSPWMChannels[channel].phaseStart;
    gSPWMChannels[channel].enabled = true;

    if (!gSPWMTimerRunning) {
        gSPWMTimerRunning = true;
        DL_TimerA_startCounter(PWM_0_INST);
    }
}

void SPWM_stop(uint8_t channel)
{
    if (!SPWM_isChannelValid(channel)) {
        return;
    }

    gSPWMChannels[channel].enabled = false;
    SPWM_writeMidpoint(channel);

    /*
     * 不立即停止 TIMA0：比较值采用零点装载，至少还要经过下一个零点，
     * 50% 占空比才能无毛刺地生效。定时器继续运行不会产生正弦交流分量。
     */
}

void SPWM_stopAll(void)
{
    gSPWMChannels[SPWM_CHANNEL_0].enabled = false;
    gSPWMChannels[SPWM_CHANNEL_1].enabled = false;
    SPWM_writeMidpoint(SPWM_CHANNEL_0);
    SPWM_writeMidpoint(SPWM_CHANNEL_1);
}

void SPWM_setFrequency(uint8_t channel, uint32_t frequencyHz)
{
    uint32_t maximumFrequency = (SPWM_UPDATE_FREQUENCY_HZ / 2U) - 1U;

    if (!SPWM_isChannelValid(channel)) {
        return;
    }
    if (frequencyHz > maximumFrequency) {
        frequencyHz = maximumFrequency;
    }

    gSPWMChannels[channel].phaseStep = (uint32_t)
        (((uint64_t) frequencyHz << 32) / SPWM_UPDATE_FREQUENCY_HZ);
}

void SPWM_setAmplitude(uint8_t channel, uint16_t amplitudePermille)
{
    if (!SPWM_isChannelValid(channel)) {
        return;
    }
    if (amplitudePermille > SPWM_AMPLITUDE_MAX) {
        amplitudePermille = SPWM_AMPLITUDE_MAX;
    }

    gSPWMChannels[channel].amplitudePermille = amplitudePermille;
}

void SPWM_setPhase(uint8_t channel, uint16_t phaseDegree)
{
    uint32_t phase;

    if (!SPWM_isChannelValid(channel)) {
        return;
    }

    phaseDegree %= 360U;
    phase = (uint32_t) ((((uint64_t) phaseDegree) << 32) / 360U);
    gSPWMChannels[channel].phaseStart = phase;
    gSPWMChannels[channel].phaseAccumulator = phase;
}

void SPWM_set(uint8_t channel, uint32_t frequencyHz,
    uint16_t amplitudePermille, uint16_t phaseDegree)
{
    SPWM_setFrequency(channel, frequencyHz);
    SPWM_setAmplitude(channel, amplitudePermille);
    SPWM_setPhase(channel, phaseDegree);
}

/* TIMA0 每到一个 PWM 周期的零点，就同步更新两路比较值。 */
void TIMA0_IRQHandler(void)
{
    if (DL_TimerA_getPendingInterrupt(PWM_0_INST) == DL_TIMER_IIDX_ZERO) {
        SPWM_updateChannel(SPWM_CHANNEL_0);
        SPWM_updateChannel(SPWM_CHANNEL_1);
    }
}
