/*
 * Copyright (c) 2021, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of Texas Instruments Incorporated nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ti_msp_dl_config.h"

#include "Drivers/adc_multi.h"
#include "Drivers/lcd_8080.h"
#include "Drivers/lcd_panel.h"
#include "Drivers/spwm.h"
#include "Graphics/adc_multi_visualization.h"

int main(void)
{
    const ADCMulti_Frame *frames;
    uint16_t frameCount;

    /* 外设和驱动只需在上电后初始化一次。 */
    SYSCFG_DL_init();
    SPWM_init();
    ADCMulti_init();
    LCDPanel_init();
    LCD8080_clear(LCD_COLOR_BLACK);

    /* 默认 10 kframe/s，便于边采集边刷新 LCD；需要时可提高到 100 k。 */
    if (!ADCMulti_start(ADC_MULTI_DEFAULT_RATE_HZ)) {
        ADCMultiVisualization_drawError("ADC START FAILED");
    }

    while (1) {
        /*
         * 非阻塞地取得已经采满的一块缓冲区。DMA 会同时写另一块，
         * LCD 刷新结束后必须及时释放当前缓冲区。
         */
        if (ADCMulti_getReadyBuffer(&frames, &frameCount)) {
            ADCMultiVisualization_draw(frames, frameCount,
                ADCMulti_getActualFrameRate(),
                ADCMulti_getOverrunCount());
            ADCMulti_releaseBuffer(frames);
        }
    }
}
