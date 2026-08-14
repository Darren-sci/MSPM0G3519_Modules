#include "Drivers/adc_multi.h"

#include "ti_msp_dl_config.h"

/* FIFO 每次读取返回两个 16 位 ADC 结果。 */
#define ADC_MULTI_SAMPLES_PER_BUFFER \
    (ADC_MULTI_FRAME_COUNT * ADC_MULTI_CHANNEL_COUNT)
#define ADC_MULTI_DMA_WORD_COUNT     (ADC_MULTI_SAMPLES_PER_BUFFER / 2U)

#define ADC_MULTI_BUFFER_0_MASK      (1U << 0)
#define ADC_MULTI_BUFFER_1_MASK      (1U << 1)

/*
 * union 同时保证 DMA 目标地址按 32 位对齐，并给上层提供按帧访问的视图。
 * MSPM0 为小端存储，FIFO 中连续的两个半字会保持原来的通道顺序。
 */
typedef union {
    ADCMulti_Frame frames[ADC_MULTI_FRAME_COUNT];
    uint32_t words[ADC_MULTI_DMA_WORD_COUNT];
} ADCMulti_Buffer;

static ADCMulti_Buffer gBuffers[2];
static volatile uint8_t gWriteIndex;
static volatile uint8_t gReadyMask;
static volatile uint8_t gAcquiredMask;
static volatile uint32_t gActualFrameRate;
static volatile uint32_t gOverrunCount;
static volatile bool gRunning;

static uint8_t ADCMulti_indexToMask(uint8_t index)
{
    return (index == 0U) ? ADC_MULTI_BUFFER_0_MASK :
                           ADC_MULTI_BUFFER_1_MASK;
}

static void ADCMulti_configureDMA(uint8_t bufferIndex)
{
    DL_DMA_disableChannel(DMA, ADC_DMA_CHAN_ID);
    DL_DMA_setSrcAddr(DMA, ADC_DMA_CHAN_ID,
        DL_ADC12_getFIFOAddress(ADC_CAPTURE_INST));
    DL_DMA_setDestAddr(DMA, ADC_DMA_CHAN_ID,
        (uint32_t)&gBuffers[bufferIndex].words[0]);
    DL_DMA_setTransferSize(
        DMA, ADC_DMA_CHAN_ID, ADC_MULTI_DMA_WORD_COUNT);
    DL_DMA_enableChannel(DMA, ADC_DMA_CHAN_ID);

    /* ADC 在一次外部 DMA 传输完成后会关闭 DMA 请求，下一块必须重新使能。 */
    DL_ADC12_enableDMA(ADC_CAPTURE_INST);
}

void ADCMulti_init(void)
{
    DL_TimerG_stopCounter(ADC_SAMPLE_TIMER_INST);
    DL_DMA_disableChannel(DMA, ADC_DMA_CHAN_ID);
    DL_ADC12_disableDMA(ADC_CAPTURE_INST);

    gWriteIndex       = 0U;
    gReadyMask        = 0U;
    gAcquiredMask     = 0U;
    gActualFrameRate  = 0U;
    gOverrunCount     = 0U;
    gRunning          = false;

    NVIC_ClearPendingIRQ(ADC_CAPTURE_INST_INT_IRQN);
    NVIC_EnableIRQ(ADC_CAPTURE_INST_INT_IRQN);
}

bool ADCMulti_start(uint32_t frameRateHz)
{
    uint32_t timerTicks;

    if ((frameRateHz < ADC_MULTI_MIN_RATE_HZ) ||
        (frameRateHz > ADC_MULTI_MAX_RATE_HZ)) {
        return false;
    }

    /* TIMG0 使用 32 MHz BUSCLK，四舍五入到最接近的整数周期。 */
    timerTicks = (CPUCLK_FREQ + frameRateHz / 2U) / frameRateHz;
    if ((timerTicks < 2U) || (timerTicks > 65536U)) {
        return false;
    }

    ADCMulti_stop();

    gWriteIndex      = 0U;
    gReadyMask       = 0U;
    gAcquiredMask    = 0U;
    gOverrunCount    = 0U;
    gActualFrameRate = CPUCLK_FREQ / timerTicks;

    DL_Timer_setLoadValue(ADC_SAMPLE_TIMER_INST, timerTicks - 1U);
    DL_Timer_setTimerCount(ADC_SAMPLE_TIMER_INST, timerTicks - 1U);

    DL_ADC12_clearInterruptStatus(
        ADC_CAPTURE_INST, DL_ADC12_INTERRUPT_DMA_DONE);
    ADCMulti_configureDMA(gWriteIndex);
    DL_ADC12_enableConversions(ADC_CAPTURE_INST);

    gRunning = true;
    DL_TimerG_startCounter(ADC_SAMPLE_TIMER_INST);
    return true;
}

void ADCMulti_stop(void)
{
    uint32_t primask = __get_PRIMASK();

    /* 防止 DMA 完成中断在停止过程中重新装载并使能 DMA。 */
    __disable_irq();
    gRunning = false;
    DL_TimerG_stopCounter(ADC_SAMPLE_TIMER_INST);
    DL_ADC12_disableConversions(ADC_CAPTURE_INST);
    DL_ADC12_disableDMA(ADC_CAPTURE_INST);
    DL_DMA_disableChannel(DMA, ADC_DMA_CHAN_ID);
    DL_ADC12_clearInterruptStatus(
        ADC_CAPTURE_INST, DL_ADC12_INTERRUPT_DMA_DONE);
    NVIC_ClearPendingIRQ(ADC_CAPTURE_INST_INT_IRQN);
    if (primask == 0U) {
        __enable_irq();
    }
}

bool ADCMulti_getReadyBuffer(
    const ADCMulti_Frame **frames, uint16_t *frameCount)
{
    uint32_t primask;
    uint8_t index;
    uint8_t mask;

    if ((frames == 0) || (frameCount == 0)) {
        return false;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    if ((gReadyMask & ADC_MULTI_BUFFER_0_MASK) != 0U) {
        index = 0U;
    } else if ((gReadyMask & ADC_MULTI_BUFFER_1_MASK) != 0U) {
        index = 1U;
    } else {
        if (primask == 0U) {
            __enable_irq();
        }
        return false;
    }

    mask = ADCMulti_indexToMask(index);
    gReadyMask &= (uint8_t)~mask;
    gAcquiredMask |= mask;

    if (primask == 0U) {
        __enable_irq();
    }

    *frames = &gBuffers[index].frames[0];
    *frameCount = ADC_MULTI_FRAME_COUNT;
    return true;
}

void ADCMulti_releaseBuffer(const ADCMulti_Frame *frames)
{
    uint32_t primask;
    uint8_t mask = 0U;

    if (frames == &gBuffers[0].frames[0]) {
        mask = ADC_MULTI_BUFFER_0_MASK;
    } else if (frames == &gBuffers[1].frames[0]) {
        mask = ADC_MULTI_BUFFER_1_MASK;
    } else {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    gAcquiredMask &= (uint8_t)~mask;
    if (primask == 0U) {
        __enable_irq();
    }
}

uint32_t ADCMulti_getActualFrameRate(void)
{
    return gActualFrameRate;
}

uint32_t ADCMulti_getOverrunCount(void)
{
    return gOverrunCount;
}

bool ADCMulti_isRunning(void)
{
    return gRunning;
}

void ADC_CAPTURE_INST_IRQHandler(void)
{
    if (DL_ADC12_getPendingInterrupt(ADC_CAPTURE_INST) ==
        DL_ADC12_IIDX_DMA_DONE) {
        uint8_t completedIndex = gWriteIndex;
        uint8_t nextIndex = (uint8_t)(completedIndex ^ 1U);
        uint8_t completedMask = ADCMulti_indexToMask(completedIndex);
        uint8_t nextMask = ADCMulti_indexToMask(nextIndex);

        if (!gRunning) {
            return;
        }

        if ((gAcquiredMask & nextMask) != 0U) {
            /*
             * CPU 仍在使用另一块缓冲区，只能立即复用刚写完的缓冲区。
             * 本块数据因此被丢弃，但不会破坏 CPU 正在读取的数据。
             */
            gOverrunCount++;
            nextIndex = completedIndex;
        } else {
            if ((gReadyMask & nextMask) != 0U) {
                /* 另一块尚未领取，丢弃更旧的一块并记录溢出。 */
                gReadyMask &= (uint8_t)~nextMask;
                gOverrunCount++;
            }
            gReadyMask |= completedMask;
        }

        gWriteIndex = nextIndex;
        ADCMulti_configureDMA(nextIndex);
    }
}
