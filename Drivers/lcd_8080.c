#include "Drivers/lcd_8080.h"

#include "BSP/board_pins.h"

static void LCD8080_delayMs(uint32_t milliseconds)
{
    while (milliseconds-- != 0U) {
        delay_cycles(CPUCLK_FREQ / 1000U);
    }
}

static inline void LCD8080_writeBus(uint16_t value)
{
    /* 先稳定16位数据总线，再通过WR低脉冲让LCD锁存当前数值。 */
    DL_GPIO_writePinsVal(
        BOARD_LCD_DATA_PORT, BOARD_LCD_DATA_MASK, (uint32_t) value);

    DL_GPIO_clearPins(BOARD_LCD_CTRL_PORT, BOARD_LCD_WR_PIN);
    /* Satisfy the LCD write-low pulse width at both 32 MHz and 80 MHz. */
    __NOP();
    __NOP();
    DL_GPIO_setPins(BOARD_LCD_CTRL_PORT, BOARD_LCD_WR_PIN);
}

void LCD8080_initBus(void)
{
    DL_GPIO_setPins(BOARD_LCD_CTRL_PORT,
        BOARD_LCD_WR_PIN | BOARD_LCD_RD_PIN | BOARD_LCD_CS_PIN |
        BOARD_LCD_RS_PIN | BOARD_LCD_RST_PIN);
    DL_GPIO_clearPins(BOARD_LCD_CTRL_PORT, BOARD_LCD_BL_PIN);

    /* The display is the only device on this parallel bus. */
    DL_GPIO_clearPins(BOARD_LCD_CTRL_PORT, BOARD_LCD_CS_PIN);
}

void LCD8080_hardwareReset(void)
{
    DL_GPIO_clearPins(BOARD_LCD_CTRL_PORT, BOARD_LCD_RST_PIN);
    LCD8080_delayMs(20U);
    DL_GPIO_setPins(BOARD_LCD_CTRL_PORT, BOARD_LCD_RST_PIN);
    LCD8080_delayMs(20U);
}

void LCD8080_setBacklight(uint8_t enabled)
{
    if (enabled != 0U) {
        DL_GPIO_setPins(BOARD_LCD_CTRL_PORT, BOARD_LCD_BL_PIN);
    } else {
        DL_GPIO_clearPins(BOARD_LCD_CTRL_PORT, BOARD_LCD_BL_PIN);
    }
}

void LCD8080_writeCommand(uint16_t command)
{
    DL_GPIO_clearPins(BOARD_LCD_CTRL_PORT, BOARD_LCD_RS_PIN);
    LCD8080_writeBus(command);
}

void LCD8080_writeData(uint16_t data)
{
    DL_GPIO_setPins(BOARD_LCD_CTRL_PORT, BOARD_LCD_RS_PIN);
    LCD8080_writeBus(data);
}

void LCD8080_writePixels(const uint16_t *pixels, uint32_t count)
{
    if (pixels == 0) {
        return;
    }

    DL_GPIO_setPins(BOARD_LCD_CTRL_PORT, BOARD_LCD_RS_PIN);
    while (count-- != 0U) {
        LCD8080_writeBus(*pixels++);
    }
}

void LCD8080_writeColor(uint16_t color, uint32_t count)
{
    DL_GPIO_setPins(BOARD_LCD_CTRL_PORT, BOARD_LCD_RS_PIN);
    while (count-- != 0U) {
        LCD8080_writeBus(color);
    }
}

void LCD8080_setWindow(
    uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    LCD8080_writeCommand(0x2AU);
    LCD8080_writeData(x0 >> 8);
    LCD8080_writeData(x0 & 0xFFU);
    LCD8080_writeData(x1 >> 8);
    LCD8080_writeData(x1 & 0xFFU);

    LCD8080_writeCommand(0x2BU);
    LCD8080_writeData(y0 >> 8);
    LCD8080_writeData(y0 & 0xFFU);
    LCD8080_writeData(y1 >> 8);
    LCD8080_writeData(y1 & 0xFFU);

    LCD8080_writeCommand(0x2CU);
    DL_GPIO_setPins(BOARD_LCD_CTRL_PORT, BOARD_LCD_RS_PIN);
}

void LCD8080_drawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if ((x >= LCD_WIDTH) || (y >= LCD_HEIGHT)) {
        return;
    }

    LCD8080_setWindow(x, y, x, y);
    LCD8080_writeColor(color, 1U);
}

void LCD8080_fillRect(uint16_t x, uint16_t y, uint16_t width,
    uint16_t height, uint16_t color)
{
    uint32_t pixelCount;

    if ((width == 0U) || (height == 0U) || (x >= LCD_WIDTH) ||
        (y >= LCD_HEIGHT)) {
        return;
    }
    if (((uint32_t) x + width) > LCD_WIDTH) {
        width = (uint16_t) (LCD_WIDTH - x);
    }
    if (((uint32_t) y + height) > LCD_HEIGHT) {
        height = (uint16_t) (LCD_HEIGHT - y);
    }

    LCD8080_setWindow(x, y, (uint16_t) (x + width - 1U),
        (uint16_t) (y + height - 1U));
    pixelCount = (uint32_t) width * height;
    LCD8080_writeColor(color, pixelCount);
}

void LCD8080_clear(uint16_t color)
{
    LCD8080_fillRect(0U, 0U, LCD_WIDTH, LCD_HEIGHT, color);
}
