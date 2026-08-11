#include "Drivers/lcd_panel.h"

#include "Drivers/lcd_8080.h"
#include "ti_msp_dl_config.h"

static void panelDelayMs(uint32_t milliseconds)
{
    while (milliseconds-- != 0U) {
        delay_cycles(CPUCLK_FREQ / 1000U);
    }
}

static void writeCommandData(
    uint16_t command, const uint8_t *data, uint32_t length)
{
    /* 先发送寄存器命令，再按顺序发送该寄存器的全部参数。 */
    LCD8080_writeCommand(command);
    while (length-- != 0U) {
        LCD8080_writeData(*data++);
    }
}

void LCDPanel_init(void)
{
    static const uint8_t extc[] = {0xFF, 0x98, 0x06};
    static const uint8_t gip1[] = {0x03, 0x0F, 0x63, 0x69, 0x01, 0x01,
        0x1B, 0x11, 0x70, 0x73, 0xFF, 0xFF, 0x08, 0x09, 0x05, 0x00,
        0xEE, 0xE2, 0x01, 0x00, 0xC1};
    static const uint8_t gip2[] =
        {0x01, 0x23, 0x45, 0x67, 0x01, 0x23, 0x45, 0x67};
    static const uint8_t gip3[] =
        {0x00, 0x22, 0x27, 0x6A, 0xBC, 0xD8, 0x92, 0x22, 0x22};
    static const uint8_t vcom[] = {0x8F, 0x80};
    static const uint8_t enVolt[] = {0x7F, 0x0F};
    static const uint8_t power1[] = {0xC7, 0x0B, 0x02, 0x00, 0x00,
        0x88, 0x2C, 0x50, 0x00, 0x00, 0x00, 0x00, 0xFF};
    static const uint8_t engineering[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x20};
    static const uint8_t inversion[] = {0x00, 0x00, 0x00};
    static const uint8_t frameRate[] = {0x00, 0x10, 0x14};
    static const uint8_t timing1[] = {0x29, 0x8A, 0x07};
    static const uint8_t timing2[] = {0x40, 0xD2, 0x50, 0x28};
    static const uint8_t power2[] = {0x17, 0x85, 0x85, 0x20};
    static const uint8_t gammaP[] = {0x00, 0x0C, 0x15, 0x0D, 0x0F, 0x0C,
        0x07, 0x05, 0x07, 0x0B, 0x10, 0x10, 0x0D, 0x17, 0x0F, 0x00};
    static const uint8_t gammaN[] = {0x00, 0x0D, 0x15, 0x0E, 0x10, 0x0D,
        0x08, 0x06, 0x07, 0x0C, 0x11, 0x11, 0x0E, 0x17, 0x0F, 0x00};
    static const uint8_t zero = 0x00;
    static const uint8_t vgl = 0x04;
    static const uint8_t dvdd = 0x74;
    static const uint8_t resolution = 0x81;
    static const uint8_t landscape = 0xA0;
    static const uint8_t rgb565 = 0x55;

    LCD8080_initBus();
    LCD8080_hardwareReset();

    writeCommandData(0xFF, extc, sizeof(extc));
    writeCommandData(0xBC, gip1, sizeof(gip1));
    writeCommandData(0xBD, gip2, sizeof(gip2));
    writeCommandData(0xBE, gip3, sizeof(gip3));
    writeCommandData(0xC7, vcom, sizeof(vcom));
    writeCommandData(0xED, enVolt, sizeof(enVolt));
    writeCommandData(0xC0, power1, sizeof(power1));
    writeCommandData(0xFC, &vgl, 1U);
    writeCommandData(0xDF, engineering, sizeof(engineering));
    writeCommandData(0xF3, &dvdd, 1U);
    writeCommandData(0xB4, inversion, sizeof(inversion));
    writeCommandData(0xF7, &resolution, 1U);
    writeCommandData(0xB1, frameRate, sizeof(frameRate));
    writeCommandData(0xF1, timing1, sizeof(timing1));
    writeCommandData(0xF2, timing2, sizeof(timing2));
    writeCommandData(0xC1, power2, sizeof(power2));
    writeCommandData(0xE0, gammaP, sizeof(gammaP));
    writeCommandData(0xE1, gammaN, sizeof(gammaN));
    writeCommandData(0x35, &zero, 1U);
    writeCommandData(0x36, &landscape, 1U);
    writeCommandData(0x3A, &rgb565, 1U);

    LCD8080_writeCommand(0x11U);
    panelDelayMs(120U);
    LCD8080_writeCommand(0x29U);
    panelDelayMs(10U);

    LCD8080_clear(LCD_COLOR_BLACK);
    LCD8080_setBacklight(1U);
}

void LCDPanel_displayOn(uint8_t enabled)
{
    LCD8080_writeCommand((enabled != 0U) ? 0x29U : 0x28U);
    LCD8080_setBacklight(enabled);
}
