#include "Drivers/system_tick.h"

#include "ti_msp_dl_config.h"

/* 该变量由定时器中断更新，因此必须使用 volatile。 */
static volatile uint32_t gSystemTimeMs;

void SystemTick_start(void)
{
    gSystemTimeMs = 0U;

    /* 启动前清除可能残留的中断状态，随后允许 TIMG7 进入 NVIC。 */
    DL_TimerG_clearInterruptStatus(
        SYSTEM_TICK_INST, DL_TIMERG_INTERRUPT_ZERO_EVENT);
    NVIC_ClearPendingIRQ(SYSTEM_TICK_INST_INT_IRQN);
    NVIC_EnableIRQ(SYSTEM_TICK_INST_INT_IRQN);
    DL_TimerG_startCounter(SYSTEM_TICK_INST);
}

uint32_t SystemTick_getMs(void)
{
    return gSystemTimeMs;
}

bool SystemTick_isDue(uint32_t *lastTimeMs, uint32_t periodMs)
{
    uint32_t now;

    if ((lastTimeMs == 0) || (periodMs == 0U)) {
        return false;
    }

    now = SystemTick_getMs();
    if ((uint32_t)(now - *lastTimeMs) < periodMs) {
        return false;
    }

    /* 按设定周期推进时间点，避免主循环轻微波动造成长期周期漂移。 */
    *lastTimeMs += periodMs;
    return true;
}

/*
 * 该函数由 TIMG7 硬件中断自动调用，主程序不应直接调用。
 * 中断中只累计时间，具体状态机和耗时任务放在主循环中执行。
 */
void SYSTEM_TICK_INST_IRQHandler(void)
{
    switch (DL_TimerG_getPendingInterrupt(SYSTEM_TICK_INST)) {
    case DL_TIMER_IIDX_ZERO:
        gSystemTimeMs++;
        break;

    default:
        break;
    }
}
