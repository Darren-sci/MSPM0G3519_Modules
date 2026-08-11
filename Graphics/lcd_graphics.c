#include "Graphics/lcd_graphics.h"

#include "Drivers/lcd_8080.h"

static int32_t absoluteValue(int32_t value)
{
    return (value < 0) ? -value : value;
}

void LCDGraphics_drawLine(int32_t x0, int32_t y0,
    int32_t x1, int32_t y1, uint16_t color)
{
    int32_t dx = absoluteValue(x1 - x0);
    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t dy = -absoluteValue(y1 - y0);
    int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t error = dx + dy;

    for (;;) {
        if ((x0 >= 0) && (y0 >= 0)) {
            LCD8080_drawPixel((uint16_t) x0, (uint16_t) y0, color);
        }
        if ((x0 == x1) && (y0 == y1)) {
            break;
        }

        {
            int32_t twiceError = 2 * error;
            if (twiceError >= dy) {
                error += dy;
                x0 += sx;
            }
            if (twiceError <= dx) {
                error += dx;
                y0 += sy;
            }
        }
    }
}

void LCDGraphics_drawRect(uint16_t x, uint16_t y,
    uint16_t width, uint16_t height, uint16_t color)
{
    if ((width == 0U) || (height == 0U)) {
        return;
    }

    LCD8080_fillRect(x, y, width, 1U, color);
    if (height > 1U) {
        LCD8080_fillRect(x, (uint16_t) (y + height - 1U), width, 1U, color);
    }
    LCD8080_fillRect(x, y, 1U, height, color);
    if (width > 1U) {
        LCD8080_fillRect((uint16_t) (x + width - 1U), y, 1U, height, color);
    }
}

void LCDGraphics_fillRect(uint16_t x, uint16_t y,
    uint16_t width, uint16_t height, uint16_t color)
{
    LCD8080_fillRect(x, y, width, height, color);
}

void LCDGraphics_drawCircle(int32_t centerX, int32_t centerY,
    int32_t radius, uint16_t color)
{
    int32_t x = radius;
    int32_t y = 0;
    int32_t error = 1 - radius;

    if (radius < 0) {
        return;
    }

    while (x >= y) {
        LCD8080_drawPixel((uint16_t) (centerX + x), (uint16_t) (centerY + y), color);
        LCD8080_drawPixel((uint16_t) (centerX + y), (uint16_t) (centerY + x), color);
        LCD8080_drawPixel((uint16_t) (centerX - y), (uint16_t) (centerY + x), color);
        LCD8080_drawPixel((uint16_t) (centerX - x), (uint16_t) (centerY + y), color);
        LCD8080_drawPixel((uint16_t) (centerX - x), (uint16_t) (centerY - y), color);
        LCD8080_drawPixel((uint16_t) (centerX - y), (uint16_t) (centerY - x), color);
        LCD8080_drawPixel((uint16_t) (centerX + y), (uint16_t) (centerY - x), color);
        LCD8080_drawPixel((uint16_t) (centerX + x), (uint16_t) (centerY - y), color);

        y++;
        if (error < 0) {
            error += 2 * y + 1;
        } else {
            x--;
            error += 2 * (y - x) + 1;
        }
    }
}

void LCDGraphics_fillCircle(int32_t centerX, int32_t centerY,
    int32_t radius, uint16_t color)
{
    int32_t y;
    int32_t x = radius;
    int32_t error = 1 - radius;

    if (radius < 0) {
        return;
    }

    for (y = 0; y <= x; y++) {
        LCDGraphics_drawLine(centerX - x, centerY + y,
            centerX + x, centerY + y, color);
        LCDGraphics_drawLine(centerX - x, centerY - y,
            centerX + x, centerY - y, color);
        LCDGraphics_drawLine(centerX - y, centerY + x,
            centerX + y, centerY + x, color);
        LCDGraphics_drawLine(centerX - y, centerY - x,
            centerX + y, centerY - x, color);

        if (error < 0) {
            error += 2 * y + 3;
        } else {
            x--;
            error += 2 * (y - x) + 3;
        }
    }
}
