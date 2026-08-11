#ifndef GRAPHICS_LCD_GRAPHICS_H_
#define GRAPHICS_LCD_GRAPHICS_H_

#include <stdint.h>

/**
 * @brief 在两个坐标点之间绘制一条直线。
 * @param x0 起点X坐标。
 * @param y0 起点Y坐标。
 * @param x1 终点X坐标。
 * @param y1 终点Y坐标。
 * @param color 直线的RGB565颜色。
 *
 * 使用Bresenham算法，支持水平线、垂直线和任意斜线。
 */
void LCDGraphics_drawLine(int32_t x0, int32_t y0,
    int32_t x1, int32_t y1, uint16_t color);

/**
 * @brief 绘制一个空心矩形边框。
 * @param x 矩形左上角X坐标。
 * @param y 矩形左上角Y坐标。
 * @param width 矩形总宽度，单位为像素。
 * @param height 矩形总高度，单位为像素。
 * @param color 边框的RGB565颜色。
 */
void LCDGraphics_drawRect(uint16_t x, uint16_t y,
    uint16_t width, uint16_t height, uint16_t color);

/**
 * @brief 绘制一个实心矩形。
 * @param x 矩形左上角X坐标。
 * @param y 矩形左上角Y坐标。
 * @param width 矩形宽度，单位为像素。
 * @param height 矩形高度，单位为像素。
 * @param color 矩形的RGB565填充颜色。
 */
void LCDGraphics_fillRect(uint16_t x, uint16_t y,
    uint16_t width, uint16_t height, uint16_t color);

/**
 * @brief 绘制一个空心圆。
 * @param centerX 圆心X坐标。
 * @param centerY 圆心Y坐标。
 * @param radius 圆的半径，单位为像素；负数无效。
 * @param color 圆周的RGB565颜色。
 */
void LCDGraphics_drawCircle(int32_t centerX, int32_t centerY,
    int32_t radius, uint16_t color);

/**
 * @brief 绘制一个实心圆。
 * @param centerX 圆心X坐标。
 * @param centerY 圆心Y坐标。
 * @param radius 圆的半径，单位为像素；负数无效。
 * @param color 圆的RGB565填充颜色。
 */
void LCDGraphics_fillCircle(int32_t centerX, int32_t centerY,
    int32_t radius, uint16_t color);

#endif
