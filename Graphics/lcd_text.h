#ifndef GRAPHICS_LCD_TEXT_H_
#define GRAPHICS_LCD_TEXT_H_

#include <stdint.h>

/**
 * @brief 单个中文或其他非ASCII字符的软件字模描述。
 *
 * bitmap按行存储，每个像素占1位，每个字节的最高位对应左侧像素。每行不足
 * 8位的部分仍占用一个完整字节，数组总大小为height * ceil(width / 8)。
 */
typedef struct {
    uint32_t codepoint;       /**< 字符的Unicode码点，例如“中”为0x4E2D。 */
    uint8_t width;            /**< 字模宽度，单位为像素。 */
    uint8_t height;           /**< 字模高度，单位为像素。 */
    const uint8_t *bitmap;    /**< 指向只读点阵数据的指针。 */
} LCDText_SoftGlyph;

/**
 * @brief 使用内置5x7 ASCII字库绘制一个字符。
 * @param x 字符左上角X坐标。
 * @param y 字符左上角Y坐标。
 * @param character 要显示的ASCII字符；无效字符会替换为问号。
 * @param foreground 字符笔画的RGB565颜色。
 * @param background 字符背景的RGB565颜色。
 * @param scale 整数放大倍数；传入0时按1倍处理。
 * @param transparent 非0表示透明背景，0表示同时绘制背景色。
 *
 * 一个字符在1倍时占用6x8像素，其中5列为字形、1列为字符间距。
 */
void LCDText_drawAsciiChar(uint16_t x, uint16_t y, char character,
    uint16_t foreground, uint16_t background,
    uint8_t scale, uint8_t transparent);

/**
 * @brief 使用内置ASCII字库绘制一个以\0结尾的字符串。
 * @param x 第一行文字的起始X坐标。
 * @param y 第一行文字的起始Y坐标。
 * @param text ASCII字符串指针，支持换行符\n。
 * @param foreground 文字的RGB565颜色。
 * @param background 背景的RGB565颜色。
 * @param scale 整数放大倍数；传入0时按1倍处理。
 * @param transparent 非0表示透明背景，0表示绘制背景色。
 */
void LCDText_drawAsciiString(uint16_t x, uint16_t y, const char *text,
    uint16_t foreground, uint16_t background,
    uint8_t scale, uint8_t transparent);

/**
 * @brief 在指定位置绘制一个调用者提供的软件字模。
 * @param x 字模左上角X坐标。
 * @param y 字模左上角Y坐标。
 * @param glyph 软件字模描述结构体指针。
 * @param foreground 字模有效像素的RGB565颜色。
 * @param background 字模背景的RGB565颜色。
 * @param scale 整数放大倍数；传入0时按1倍处理。
 * @param transparent 非0表示透明背景，0表示绘制背景色。
 */
void LCDText_drawSoftGlyph(uint16_t x, uint16_t y,
    const LCDText_SoftGlyph *glyph, uint16_t foreground,
    uint16_t background, uint8_t scale, uint8_t transparent);

/**
 * @brief 绘制同时包含ASCII和中文的UTF-8字符串。
 * @param x 第一行文字的起始X坐标。
 * @param y 第一行文字的起始Y坐标。
 * @param utf8 以\0结尾的UTF-8字符串，支持换行符\n。
 * @param glyphs 中文或其他非ASCII字符的软件字模表。
 * @param glyphCount glyphs表中的有效字模数量。
 * @param foreground 文字的RGB565颜色。
 * @param background 背景的RGB565颜色。
 * @param scale 整数放大倍数；传入0时按1倍处理。
 * @param transparent 非0表示透明背景，0表示绘制背景色。
 *
 * ASCII字符使用模块内置字库；非ASCII字符根据Unicode码点在glyphs中查找。
 * 找不到对应字模时会绘制一个16x16的空心方框。
 */
void LCDText_drawUtf8(uint16_t x, uint16_t y, const char *utf8,
    const LCDText_SoftGlyph *glyphs, uint32_t glyphCount,
    uint16_t foreground, uint16_t background,
    uint8_t scale, uint8_t transparent);

#endif
