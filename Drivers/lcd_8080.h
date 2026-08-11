#ifndef DRIVERS_LCD_8080_H_
#define DRIVERS_LCD_8080_H_

#include <stdint.h>

#define LCD_WIDTH                  (854U)
#define LCD_HEIGHT                 (480U)

/* RGB565 colors. */
#define LCD_COLOR_BLACK            (0x0000U)
#define LCD_COLOR_WHITE            (0xFFFFU)
#define LCD_COLOR_RED              (0xF800U)
#define LCD_COLOR_GREEN            (0x07E0U)
#define LCD_COLOR_BLUE             (0x001FU)
#define LCD_COLOR_CYAN             (0x07FFU)
#define LCD_COLOR_MAGENTA          (0xF81FU)
#define LCD_COLOR_YELLOW           (0xFFE0U)
#define LCD_COLOR_DARK_GRAY        (0x2104U)
#define LCD_COLOR_GRAY             (0x8410U)

/**
 * @brief 初始化LCD的16位8080并口总线电平。
 *
 * 设置WR、RD、CS、RS、RST和BL的初始状态，并拉低CS选中屏幕。
 * 一般不需要由用户单独调用，LCDPanel_init()内部会自动调用。
 */
void LCD8080_initBus(void);

/**
 * @brief 通过RST引脚对LCD进行硬件复位。
 *
 * 函数会将RST拉低20ms，再拉高并等待20ms。调用前必须已经完成GPIO初始化。
 */
void LCD8080_hardwareReset(void);

/**
 * @brief 控制LCD背光。
 * @param enabled 非0表示打开背光，0表示关闭背光。
 *
 * 关闭背光不会清除LCD内部显存中的图像。
 */
void LCD8080_setBacklight(uint8_t enabled);

/**
 * @brief 向LCD控制器写入一条命令。
 * @param command LCD控制器命令，通常使用低8位，例如0x2A、0x2B、0x2C。
 */
void LCD8080_writeCommand(uint16_t command);

/**
 * @brief 向LCD控制器写入一个参数或一个16位数据。
 * @param data 要写入的数据。寄存器参数通常使用低8位，像素使用完整16位。
 */
void LCD8080_writeData(uint16_t data);

/**
 * @brief 向当前LCD显存窗口连续写入一组RGB565像素。
 * @param pixels RGB565像素数组指针，像素按从左到右、从上到下排列。
 * @param count 要写入的像素数量，不是字节数量。
 *
 * 调用前应先使用LCD8080_setWindow()设置写入区域。
 */
void LCD8080_writePixels(const uint16_t *pixels, uint32_t count);

/**
 * @brief 向当前LCD显存窗口连续写入同一种颜色。
 * @param color RGB565颜色值，例如LCD_COLOR_RED。
 * @param count 要写入的像素数量。
 *
 * 主要用于清屏和矩形区域的快速纯色填充。
 */
void LCD8080_writeColor(uint16_t color, uint32_t count);

/**
 * @brief 设置LCD显存的矩形写入窗口，并进入显存写入状态。
 * @param x0 矩形左上角X坐标，范围0到LCD_WIDTH-1。
 * @param y0 矩形左上角Y坐标，范围0到LCD_HEIGHT-1。
 * @param x1 矩形右下角X坐标，该坐标包含在窗口内。
 * @param y1 矩形右下角Y坐标，该坐标包含在窗口内。
 *
 * 要求x0不大于x1且y0不大于y1，调用者应保证坐标有效。
 */
void LCD8080_setWindow(uint16_t x0, uint16_t y0,
    uint16_t x1, uint16_t y1);

/**
 * @brief 在指定坐标绘制一个RGB565像素。
 * @param x 像素X坐标，范围0到853。
 * @param y 像素Y坐标，范围0到479。
 * @param color RGB565颜色值。
 *
 * 坐标超出屏幕范围时函数直接返回，不进行绘制。
 */
void LCD8080_drawPixel(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief 使用指定颜色填充一个实心矩形区域。
 * @param x 矩形左上角X坐标。
 * @param y 矩形左上角Y坐标。
 * @param width 矩形宽度，单位为像素。
 * @param height 矩形高度，单位为像素。
 * @param color RGB565填充颜色。
 *
 * 超出屏幕右边或下边的部分会被自动裁剪。
 */
void LCD8080_fillRect(uint16_t x, uint16_t y, uint16_t width,
    uint16_t height, uint16_t color);

/**
 * @brief 使用指定颜色填充整个854x480屏幕。
 * @param color RGB565背景颜色。
 */
void LCD8080_clear(uint16_t color);

#endif
