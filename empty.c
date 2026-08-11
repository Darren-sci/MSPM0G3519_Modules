/*
 * Copyright (c) 2021, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ti_msp_dl_config.h"
#include "Drivers/lcd_8080.h"
#include "Drivers/lcd_panel.h"
#include "Graphics/lcd_graphics.h"
#include "Graphics/lcd_text.h"

int main(void)
{
    SYSCFG_DL_init();

    /*
     * Minimal LCD bring-up. No application layer is required: after reset the
     * panel should show three color bars, a white frame and an ASCII message.
     */
    LCDPanel_init();
    LCD8080_clear(LCD_COLOR_BLACK);
    LCD8080_fillRect(0U, 0U, LCD_WIDTH / 3U, 40U, LCD_COLOR_RED);
    LCD8080_fillRect(LCD_WIDTH / 3U, 0U, LCD_WIDTH / 3U, 40U,
        LCD_COLOR_GREEN);
    LCD8080_fillRect((LCD_WIDTH / 3U) * 2U, 0U,
        (uint16_t) (LCD_WIDTH - (LCD_WIDTH / 3U) * 2U), 40U,
        LCD_COLOR_BLUE);
    LCDGraphics_drawRect(20U, 70U, 500U, 90U, LCD_COLOR_WHITE);
    LCDText_drawAsciiString(40U, 100U, "MSPM0G3519 LCD READY",
        LCD_COLOR_YELLOW, LCD_COLOR_BLACK, 2U, 0U);

    while (1) {
        __WFI();
    }
}
