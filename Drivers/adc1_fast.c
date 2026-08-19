#include "Drivers/adc1_fast.h"

#include "ti_msp_dl_config.h"

/* FIFO把两个12位结果打包为一个32位字，DMA传输数量按字计算。 */
#define ADC1_FAST_DMA_WORD_COUNT    (ADC1_FAST_SAMPLE_COUNT / 2U)

#define ADC1_FAST_BUFFER_0_MASK     (1U << 0)
#define ADC1_FAST_BUFFER_1_MASK     (1U << 1)

/*
 * union保证DMA目标地址按32位对齐，同时允许上层按uint16_t读取ADC结果。
 * MSPM0为小端存储，FIFO中的先后两个结果会落入相邻的两个半字。
 */
typedef union {
    uint16_t samples[ADC1_FAST_SAMPLE_COUNT];
    uint32_t words[ADC1_FAST_DMA_WORD_COUNT];
} ADC1Fast_Buffer;

static ADC1Fast_Buffer gADC1FastBuffers[2];
static volatile uint8_t gADC1FastWriteIndex;
static volatile uint8_t gADC1FastReadyMask;
static volatile uint8_t gADC1FastAcquiredMask;
static volatile uint32_t gADC1FastOverrunCount;
static volatile bool gADC1FastRunning;
static bool gADC1FastInitialized;

static uint8_t ADC1Fast_indexToMask(uint8_t index)
{
    return (index == 0U) ? ADC1_FAST_BUFFER_0_MASK :
                           ADC1_FAST_BUFFER_1_MASK;
}

/** 给DMA装载下一块目标地址，并重新打开ADC的DMA请求。 */
static void ADC1Fast_configureDMA(uint8_t bufferIndex)
{
    DL_DMA_disableChannel(DMA, ADC1_FAST_DMA_CHAN_ID);
    DL_DMA_setSrcAddr(DMA, ADC1_FAST_DMA_CHAN_ID,
        DL_ADC12_getFIFOAddress(ADC1_FAST_INST));
    DL_DMA_setDestAddr(DMA, ADC1_FAST_DMA_CHAN_ID,
        (uint32_t)&gADC1FastBuffers[bufferIndex].words[0]);
    DL_DMA_setTransferSize(
        DMA, ADC1_FAST_DMA_CHAN_ID, ADC1_FAST_DMA_WORD_COUNT);
    DL_DMA_enableChannel(DMA, ADC1_FAST_DMA_CHAN_ID);

    /* 每完成一次DMA块传输，ADC会关闭DMA请求，下一块必须重新使能。 */
    DL_ADC12_enableDMA(ADC1_FAST_INST);
}

void ADC1Fast_init(void)
{
    DL_ADC12_stopConversion(ADC1_FAST_INST);
    DL_ADC12_disableConversions(ADC1_FAST_INST);
    DL_ADC12_disableDMA(ADC1_FAST_INST);
    DL_DMA_disableChannel(DMA, ADC1_FAST_DMA_CHAN_ID);

    gADC1FastWriteIndex   = 0U;
    gADC1FastReadyMask    = 0U;
    gADC1FastAcquiredMask = 0U;
    gADC1FastOverrunCount = 0U;
    gADC1FastRunning      = false;

    DL_ADC12_clearInterruptStatus(
        ADC1_FAST_INST, DL_ADC12_INTERRUPT_DMA_DONE);
    NVIC_ClearPendingIRQ(ADC1_FAST_INST_INT_IRQN);
    NVIC_EnableIRQ(ADC1_FAST_INST_INT_IRQN);
    gADC1FastInitialized = true;
}

bool ADC1Fast_start(void)
{
    if (!gADC1FastInitialized) {
        return false;
    }

    ADC1Fast_stop();

    gADC1FastWriteIndex   = 0U;
    gADC1FastReadyMask    = 0U;
    gADC1FastAcquiredMask = 0U;
    gADC1FastOverrunCount = 0U;

    /*
     * 停止发生在半个FIFO字时，关闭再打开FIFO可避免把停止前的残留结果
     * 混入新采集块。此操作必须在ADC停止转换后进行。
     */
    DL_ADC12_disableFIFO(ADC1_FAST_INST);
    DL_ADC12_enableFIFO(ADC1_FAST_INST);

    DL_ADC12_clearInterruptStatus(
        ADC1_FAST_INST, DL_ADC12_INTERRUPT_DMA_DONE);
    NVIC_ClearPendingIRQ(ADC1_FAST_INST_INT_IRQN);
    ADC1Fast_configureDMA(gADC1FastWriteIndex);

    DL_ADC12_enableConversions(ADC1_FAST_INST);
    gADC1FastRunning = true;
    DL_ADC12_startConversion(ADC1_FAST_INST);
    return true;
}

void ADC1Fast_stop(void)
{
    uint32_t primask = __get_PRIMASK();

    /* 防止DMA完成中断在停止过程中重新装载并再次启动DMA。 */
    __disable_irq();
    gADC1FastRunning = false;
    DL_ADC12_stopConversion(ADC1_FAST_INST);
    DL_ADC12_disableConversions(ADC1_FAST_INST);
    DL_ADC12_disableDMA(ADC1_FAST_INST);
    DL_DMA_disableChannel(DMA, ADC1_FAST_DMA_CHAN_ID);
    DL_ADC12_clearInterruptStatus(
        ADC1_FAST_INST, DL_ADC12_INTERRUPT_DMA_DONE);
    NVIC_ClearPendingIRQ(ADC1_FAST_INST_INT_IRQN);
    if (primask == 0U) {
        __enable_irq();
    }
}

bool ADC1Fast_getReadyBuffer(
    const uint16_t **samples, uint16_t *sampleCount)
{
    uint32_t primask;
    uint8_t index;
    uint8_t mask;

    if ((samples == 0) || (sampleCount == 0)) {
        return false;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    if ((gADC1FastReadyMask & ADC1_FAST_BUFFER_0_MASK) != 0U) {
        index = 0U;
    } else if ((gADC1FastReadyMask & ADC1_FAST_BUFFER_1_MASK) != 0U) {
        index = 1U;
    } else {
        if (primask == 0U) {
            __enable_irq();
        }
        return false;
    }

    mask = ADC1Fast_indexToMask(index);
    gADC1FastReadyMask &= (uint8_t)~mask;
    gADC1FastAcquiredMask |= mask;

    if (primask == 0U) {
        __enable_irq();
    }

    *samples = &gADC1FastBuffers[index].samples[0];
    *sampleCount = ADC1_FAST_SAMPLE_COUNT;
    return true;
}

void ADC1Fast_releaseBuffer(const uint16_t *samples)
{
    uint32_t primask;
    uint8_t mask = 0U;

    if (samples == &gADC1FastBuffers[0].samples[0]) {
        mask = ADC1_FAST_BUFFER_0_MASK;
    } else if (samples == &gADC1FastBuffers[1].samples[0]) {
        mask = ADC1_FAST_BUFFER_1_MASK;
    } else {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    gADC1FastAcquiredMask &= (uint8_t)~mask;
    if (primask == 0U) {
        __enable_irq();
    }
}

uint32_t ADC1Fast_getOverrunCount(void)
{
    return gADC1FastOverrunCount;
}

bool ADC1Fast_isRunning(void)
{
    return gADC1FastRunning;
}

void ADC1_FAST_INST_IRQHandler(void)
{
    if (DL_ADC12_getPendingInterrupt(ADC1_FAST_INST) ==
        DL_ADC12_IIDX_DMA_DONE) {
        uint8_t completedIndex = gADC1FastWriteIndex;
        uint8_t nextIndex = (uint8_t)(completedIndex ^ 1U);
        uint8_t completedMask = ADC1Fast_indexToMask(completedIndex);
        uint8_t nextMask = ADC1Fast_indexToMask(nextIndex);

        if (!gADC1FastRunning) {
            return;
        }

        if ((gADC1FastAcquiredMask & nextMask) != 0U) {
            /*
             * CPU仍在使用另一块，只能立即复用刚写完的缓冲区。刚完成的
             * 数据块不会交给上层，并记录一次软件溢出。
             */
            gADC1FastOverrunCount++;
            nextIndex = completedIndex;
        } else {
            if ((gADC1FastReadyMask & nextMask) != 0U) {
                /* 另一块尚未被领取，丢弃更旧的数据，保留最新一块。 */
                gADC1FastReadyMask &= (uint8_t)~nextMask;
                gADC1FastOverrunCount++;
            }
            gADC1FastReadyMask |= completedMask;
        }

        gADC1FastWriteIndex = nextIndex;
        ADC1Fast_configureDMA(nextIndex);
    }
}
