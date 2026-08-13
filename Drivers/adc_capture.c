#include "Drivers/adc_capture.h"

#include "ti_msp_dl_config.h"

static uint16_t gSamples[ADC_CAPTURE_SAMPLE_COUNT];

void ADCCapture_init(void)
{
    /*
     * SysConfig已经配置DMA模式、数据宽度和ADC触发源。
     * 这里补充运行时才能确定的DMA源地址。
     */
    DL_ADC12_stopConversion(ADC_CAPTURE_INST);
    DL_DMA_disableChannel(DMA, ADC_DMA_CHAN_ID);

    DL_DMA_setSrcAddr(
        DMA,
        ADC_DMA_CHAN_ID,
        DL_ADC12_getMemResultAddress(
            ADC_CAPTURE_INST,
            ADC_CAPTURE_ADCMEM_0));
}

bool ADCCapture_acquire(const uint16_t **samples)
{
    uint32_t timeout = CPUCLK_FREQ / 4U;

    if (samples == 0) {
        return false;
    }

    DL_ADC12_stopConversion(ADC_CAPTURE_INST);
    DL_DMA_disableChannel(DMA, ADC_DMA_CHAN_ID);

    /*
     * 每次采集都重新装载目标地址和传输数量。
     * DMA完成上一次传输后不会自动恢复这些值。
     */
    DL_DMA_setDestAddr(
        DMA,
        ADC_DMA_CHAN_ID,
        (uint32_t)&gSamples[0]);

    DL_DMA_setTransferSize(
        DMA,
        ADC_DMA_CHAN_ID,
        ADC_CAPTURE_SAMPLE_COUNT);

    /*
     * 必须先使能DMA，再启动ADC，防止丢失最开始的ADC结果。
     */
    DL_DMA_enableChannel(DMA, ADC_DMA_CHAN_ID);
    DL_ADC12_startConversion(ADC_CAPTURE_INST);

    while (DL_DMA_isChannelEnabled(DMA, ADC_DMA_CHAN_ID) &&
           (timeout != 0U)) {
        timeout--;
        __NOP();
    }

    DL_ADC12_stopConversion(ADC_CAPTURE_INST);

    if (timeout == 0U) {
        DL_DMA_disableChannel(DMA, ADC_DMA_CHAN_ID);
        *samples = 0;
        return false;
    }

    *samples = gSamples;
    return true;
}