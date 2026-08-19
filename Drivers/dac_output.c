#include "Drivers/dac_output.h"

#include "ti_msp_dl_config.h"

static volatile uint32_t gDACOutputUnderrunCount;
static volatile bool gDACOutputWaveformRunning;
static uint32_t gDACOutputSampleRateHz;
static uint16_t gDACOutputTableLength;
static bool gDACOutputInitialized;

/** 将驱动公开的数值型采样率换成DriverLib寄存器枚举。 */
static bool DACOutput_convertSampleRate(DACOutput_SampleRate input,
    DL_DAC12_SAMPLES_PER_SECOND *output)
{
    if (output == 0) {
        return false;
    }

    switch (input) {
        case DAC_OUTPUT_RATE_500_HZ:
            *output = DL_DAC12_SAMPLES_PER_SECOND_500;
            break;
        case DAC_OUTPUT_RATE_1_KHZ:
            *output = DL_DAC12_SAMPLES_PER_SECOND_1K;
            break;
        case DAC_OUTPUT_RATE_2_KHZ:
            *output = DL_DAC12_SAMPLES_PER_SECOND_2K;
            break;
        case DAC_OUTPUT_RATE_4_KHZ:
            *output = DL_DAC12_SAMPLES_PER_SECOND_4K;
            break;
        case DAC_OUTPUT_RATE_8_KHZ:
            *output = DL_DAC12_SAMPLES_PER_SECOND_8K;
            break;
        case DAC_OUTPUT_RATE_16_KHZ:
            *output = DL_DAC12_SAMPLES_PER_SECOND_16K;
            break;
        case DAC_OUTPUT_RATE_100_KHZ:
            *output = DL_DAC12_SAMPLES_PER_SECOND_100K;
            break;
        case DAC_OUTPUT_RATE_200_KHZ:
            *output = DL_DAC12_SAMPLES_PER_SECOND_200K;
            break;
        case DAC_OUTPUT_RATE_500_KHZ:
            *output = DL_DAC12_SAMPLES_PER_SECOND_500K;
            break;
        case DAC_OUTPUT_RATE_1_MHZ:
            *output = DL_DAC12_SAMPLES_PER_SECOND_1M;
            break;
        default:
            return false;
    }
    return true;
}

void DACOutput_init(void)
{
    /*
     * SysConfig负责寄存器基础配置和DMA通道映射；驱动上电后立即停掉
     * 采样发生器，确保只有显式调用startWaveform()才会持续运行。
     */
    DL_DAC12_disableSampleTimeGenerator(DAC0);
    DL_DAC12_disableDMATrigger(DAC0);
    DL_DMA_disableChannel(DMA, DAC_DMA_CHAN_ID);
    DL_DAC12_disableFIFO(DAC0);

    gDACOutputUnderrunCount = 0U;
    gDACOutputWaveformRunning = false;
    gDACOutputSampleRateHz = 0U;
    gDACOutputTableLength = 0U;

    DL_DAC12_clearInterruptStatus(
        DAC0, DL_DAC12_INTERRUPT_FIFO_UNDERRUN);
    NVIC_ClearPendingIRQ(DAC12_INT_IRQN);
    NVIC_EnableIRQ(DAC12_INT_IRQN);

    DL_DAC12_enable(DAC0);
    DL_DAC12_output12(DAC0, 0U);
    gDACOutputInitialized = true;
}

bool DACOutput_setCode(uint16_t code)
{
    if (!gDACOutputInitialized || (code > DAC_OUTPUT_MAX_CODE)) {
        return false;
    }

    DACOutput_stopWaveform();
    DL_DAC12_output12(DAC0, code);
    return true;
}

bool DACOutput_setMilliVolts(
    uint32_t outputMilliVolts, uint32_t referenceMilliVolts)
{
    uint32_t code;

    if ((referenceMilliVolts == 0U) ||
        (outputMilliVolts > referenceMilliVolts)) {
        return false;
    }

    code = (uint32_t)
        (((uint64_t) outputMilliVolts * DAC_OUTPUT_MAX_CODE +
          referenceMilliVolts / 2U) / referenceMilliVolts);
    return DACOutput_setCode((uint16_t) code);
}

bool DACOutput_startWaveform(const uint16_t *table,
    uint16_t tableLength, DACOutput_SampleRate sampleRate)
{
    DL_DAC12_SAMPLES_PER_SECOND driverlibRate;
    uint16_t index;

    if (!gDACOutputInitialized || (table == 0) ||
        (tableLength < 2U) ||
        (tableLength > DAC_OUTPUT_MAX_TABLE_LENGTH) ||
        !DACOutput_convertSampleRate(sampleRate, &driverlibRate)) {
        return false;
    }

    /* 启动前检查整张表，防止高位控制标志被误写进DAC数据寄存器。 */
    for (index = 0U; index < tableLength; index++) {
        if (table[index] > DAC_OUTPUT_MAX_CODE) {
            return false;
        }
    }

    DACOutput_stopWaveform();

    /* 重新打开FIFO，以清除上一次停止时可能残留的波形数据。 */
    DL_DAC12_disableFIFO(DAC0);
    DL_DAC12_enableFIFO(DAC0);
    DL_DAC12_setFIFOTriggerSource(
        DAC0, DL_DAC12_FIFO_TRIGGER_SAMPLETIMER);
    DL_DAC12_setFIFOThreshold(
        DAC0, DL_DAC12_FIFO_THRESHOLD_TWO_QTRS_EMPTY);
    DL_DAC12_setSampleRate(DAC0, driverlibRate);

    DL_DMA_disableChannel(DMA, DAC_DMA_CHAN_ID);
    DL_DMA_setSrcAddr(
        DMA, DAC_DMA_CHAN_ID, (uint32_t)&table[0]);
    DL_DMA_setDestAddr(
        DMA, DAC_DMA_CHAN_ID, (uint32_t)&DAC0->DATA0);
    DL_DMA_setTransferSize(DMA, DAC_DMA_CHAN_ID, tableLength);

    gDACOutputUnderrunCount = 0U;
    DL_DAC12_clearInterruptStatus(
        DAC0, DL_DAC12_INTERRUPT_FIFO_UNDERRUN);
    NVIC_ClearPendingIRQ(DAC12_INT_IRQN);

    /*
     * 先使能DMA和FIFO请求，让FIFO得到数据，再启动内部采样发生器输出，
     * 可降低波形开始瞬间发生FIFO欠载的概率。
     */
    DL_DMA_enableChannel(DMA, DAC_DMA_CHAN_ID);
    DL_DAC12_enableDMATrigger(DAC0);

    gDACOutputSampleRateHz = (uint32_t) sampleRate;
    gDACOutputTableLength = tableLength;
    gDACOutputWaveformRunning = true;
    DL_DAC12_enableSampleTimeGenerator(DAC0);
    return true;
}

void DACOutput_stopWaveform(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    gDACOutputWaveformRunning = false;
    DL_DAC12_disableSampleTimeGenerator(DAC0);
    DL_DAC12_disableDMATrigger(DAC0);
    DL_DMA_disableChannel(DMA, DAC_DMA_CHAN_ID);
    DL_DAC12_disableFIFO(DAC0);
    gDACOutputSampleRateHz = 0U;
    gDACOutputTableLength = 0U;
    if (primask == 0U) {
        __enable_irq();
    }
}

uint64_t DACOutput_getWaveformFrequencyMilliHz(void)
{
    if (!gDACOutputWaveformRunning ||
        (gDACOutputTableLength == 0U)) {
        return 0U;
    }

    return ((uint64_t) gDACOutputSampleRateHz * 1000U +
            gDACOutputTableLength / 2U) /
           gDACOutputTableLength;
}

uint32_t DACOutput_getUnderrunCount(void)
{
    return gDACOutputUnderrunCount;
}

bool DACOutput_isWaveformRunning(void)
{
    return gDACOutputWaveformRunning;
}

void DAC12_IRQHandler(void)
{
    if (DL_DAC12_getPendingInterrupt(DAC0) ==
        DL_DAC12_IIDX_FIFO_UNDERRUN) {
        gDACOutputUnderrunCount++;
    }
}
