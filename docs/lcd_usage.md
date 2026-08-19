# MSPM0G3519 LCD module usage

## 1. Hardware and display mode

The driver targets the same 5-inch LCD used by the MSPM0G3507 project:

- Native panel resolution: 480 x 854
- Software orientation: 854 x 480 landscape
- Interface: 16-bit 8080 MCU parallel bus
- Pixel format: RGB565
- LCD logic level: 3.3 V
- Write-only operation: `RD` stays high

Hold the board with the debug connector at the top and the two pushbuttons at
the bottom when using the physical-position descriptions below.

## 2. Wiring

### 2.1 Data bus

| LCD | MSPM0G3519 | LQFP100 pin | Board position |
| --- | --- | ---: | --- |
| D00 | PB0 | 20 | Upper-left long header, row 3 right |
| D01 | PB1 | 21 | Upper-left long header, row 3 left |
| D02 | PB2 | 23 | Upper-left long header, row 4 left |
| D03 | PB3 | 24 | Upper-left long header, row 5 right |
| D04 | PB4 | 25 | Upper-left long header, row 5 left |
| D05 | PB5 | 26 | Upper-left long header, row 6 right |
| D06 | PB6 | 40 | Upper-left long header, row 13 left |
| D07 | PB7 | 41 | Upper-left long header, row 13 right |
| D08 | PB8 | 42 | Upper-left long header, row 14 left |
| D09 | PB9 | 43 | Upper-left long header, row 14 right |
| D10 | PB10 | 44 | Upper-left long header, row 15 left |
| D11 | PB11 | 45 | Upper-left long header, row 15 right |
| D12 | PB12 | 46 | Lower-left header, row 1 left |
| D13 | PB13 | 47 | Lower-left header, row 1 right |
| D14 | PB14 | 48 | Lower-left header, row 2 left |
| D15 | PB15 | 49 | Lower-left header, row 2 right |

All 16 data pins are GPIOB bits 0 through 15. The driver therefore writes a
complete pixel using one masked GPIO port operation.

### 2.2 Control and power

| LCD | MSPM0G3519 | LQFP100 pin | Board position |
| --- | --- | ---: | --- |
| WR | PA8 | 27 | Upper-left long header, row 6 left |
| RD | PA9 | 28 | Upper-left long header, row 7 right |
| CS | PA12 | 51 | Lower-left header, row 3 right |
| RS / DC | PA13 | 52 | Lower-left header, row 4 left |
| RST | PA14 | 53 | Lower-left header, row 4 right |
| BL | PB16 | 50 | Lower-left header, row 3 left; PA15 is DAC0_OUT |
| VCC | 3V3 | - | Lower-left header, last row right |
| GND | GND | - | Lower-left header, last row left |

Leave the LCD `D16` through `D23`, touch-controller pins, SPI pins and `NC`
unconnected unless a later module explicitly uses them. Connect all LCD ground
pins to board ground.

## 3. Source structure

| Path | Responsibility |
| --- | --- |
| `BSP/board_pins.h` | Board-level names for SysConfig-generated pins |
| `Drivers/lcd_8080.c/.h` | GPIO bus, command/data writes and block pixel writes |
| `Drivers/lcd_panel.c/.h` | Controller register sequence and display power |
| `Graphics/lcd_graphics.c/.h` | Lines, rectangles and circles |
| `Graphics/lcd_text.c/.h` | ASCII and UTF-8 software-font rendering |
| `empty.syscfg` | MSPM0G3519 GPIO and initial output levels |

No ADC, oscilloscope UI, application layer, touch controller or external
W25Q64 driver is included.

## 4. Initialization

SysConfig GPIO initialization must happen before panel initialization:

```c
#include "ti_msp_dl_config.h"
#include "Drivers/lcd_panel.h"

int main(void)
{
    SYSCFG_DL_init();
    LCDPanel_init();

    while (1) {
        __WFI();
    }
}
```

`LCDPanel_init()` establishes safe bus levels, applies a hardware reset,
writes the validated power/timing/gamma settings, selects landscape RGB565,
exits sleep, clears to black, and enables the backlight.

The checked-in `empty.c` contains a bring-up screen. A successful first boot
shows red, green and blue bars, a white rectangle, and the text
`MSPM0G3519 LCD READY`.

## 5. Basic drawing functions

```c
#include "Drivers/lcd_8080.h"
#include "Graphics/lcd_graphics.h"

LCD8080_clear(LCD_COLOR_BLACK);
LCD8080_drawPixel(10, 10, LCD_COLOR_WHITE);
LCD8080_fillRect(20, 20, 160, 80, LCD_COLOR_BLUE);
LCDGraphics_drawLine(20, 120, 500, 120, LCD_COLOR_YELLOW);
LCDGraphics_drawRect(20, 150, 300, 100, LCD_COLOR_WHITE);
LCDGraphics_drawCircle(500, 250, 60, LCD_COLOR_CYAN);
LCDGraphics_fillCircle(680, 250, 40, LCD_COLOR_GREEN);
```

For a prepared RGB565 image block:

```c
LCD8080_setWindow(x, y, x + width - 1, y + height - 1);
LCD8080_writePixels(imagePixels, (uint32_t) width * height);
```

## 6. ASCII text

ASCII characters from `0x20` through `0x7F` use the built-in 5 x 7 font.
The sixth column provides spacing and `scale` enlarges each pixel.

```c
LCDText_drawAsciiString(30, 40, "Frequency: 1000 Hz",
    LCD_COLOR_WHITE, LCD_COLOR_BLACK,
    2,     /* scale */
    0);    /* opaque background */
```

Set the final argument to `1` for a transparent background.

## 7. Chinese software font

Chinese characters are constant bitmap arrays stored in MCU Flash. They do
not require W25Q64. Source strings use UTF-8 and each table entry uses the
corresponding Unicode code point.

A 16 x 16 glyph requires 32 bytes: two bytes per row, MSB first.

```c
static const uint8_t glyph_zhong[32] = {
    /* Replace with 16 rows exported by the font-generation tool. */
    0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
    0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
    0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
    0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00
};

static const LCDText_SoftGlyph chineseGlyphs[] = {
    {0x4E2D, 16, 16, glyph_zhong} /* U+4E2D: Chinese character 'zhong' */
};

LCDText_drawUtf8(30, 100, "Chinese UTF-8 text goes here",
    chineseGlyphs,
    sizeof(chineseGlyphs) / sizeof(chineseGlyphs[0]),
    LCD_COLOR_YELLOW, LCD_COLOR_BLACK, 1, 0);
```

The `utf8` argument can directly contain Chinese UTF-8 source text. If a
non-ASCII character is missing from the supplied table, the driver draws a
16 x 16 outline box. Only characters used by the final interface need to be
included, which keeps Flash usage predictable.

## 8. Bring-up checklist

If the backlight is on but the screen is blank:

1. Confirm LCD and board share ground and the LCD is powered from 3.3 V.
2. Check `RST` goes low and then high during startup.
3. Check `CS` is low and `RD` remains high.
4. Check `WR` pulses when clearing the screen.
5. Recheck `RS/DC`; swapping it with `CS` prevents initialization.
6. Keep the data wires short and verify D00-D15 are not reversed.
7. If long wires cause unstable colors, increase the `__NOP()` count in
   `LCD8080_writeBus()` before attempting a higher refresh speed.
