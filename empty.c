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

#include "Drivers/adc_capture.h"
#include "Drivers/lcd_8080.h"
#include "Drivers/lcd_panel.h"
#include "Drivers/spwm.h"
#include "Graphics/adc_visualization.h"

int main(void)
{
    const uint16_t *samples;

    /* 外设和驱动只需在上电后初始化一次。 */
    SYSCFG_DL_init();
    SPWM_init();
    ADCCapture_init();
    LCDPanel_init();
    LCD8080_clear(LCD_COLOR_BLACK);

    while (1) {
        /* KEY1 is active-low because it uses an internal pull-up. */
        if (DL_GPIO_readPins(KEYS_PORT, KEYS_KEY1_PIN) == 0U) {
            LCD8080_clear(LCD_COLOR_RED);
        } else {
            LCD8080_clear(LCD_COLOR_BLACK);
        }

        /* 每采集完一批4096点，就使用最新数据刷新屏幕上的数字。 */
        if (ADCCapture_acquire(&samples)) {
            ADCVisualization_drawData(
                samples, ADC_CAPTURE_SAMPLE_COUNT);
        } else {
            ADCVisualization_drawData(0, 0U);
        }
    }
}
