#ifndef DRIVERS_OLED_SSD1315_H_
#define DRIVERS_OLED_SSD1315_H_

#include <stdbool.h>
#include <stdint.h>

/* 0.96英寸SSD1315模块的固定逻辑分辨率。 */
#define OLED_WIDTH                 (128U)
#define OLED_HEIGHT                (64U)
#define OLED_PAGE_COUNT            (OLED_HEIGHT / 8U)

/*
 * 字体使用同一份5x7 ASCII点阵进行整数倍放大。
 * 每个字符实际占用6x8、12x16或18x24像素，其中包含1列字符间距。
 */
typedef enum {
    OLED_FONT_6X8 = 1,
    OLED_FONT_12X16 = 2,
    OLED_FONT_18X24 = 3
} OLED_Font;

/**
 * @brief 初始化SSD1315并清除物理屏幕。
 *
 * 必须先调用SYSCFG_DL_init()，由SysConfig初始化OLED_I2C。
 * 初始化过程会检查I2C应答；屏幕未连接或总线异常时返回false。
 */
bool OLED_Init(void);

/** 清空RAM显存，但暂不发送I2C数据。 */
void OLED_ClearBuffer(void);

/** 清空RAM显存并立即刷新到屏幕。 */
bool OLED_ClearScreen(void);

/** 清除矩形区域；超出屏幕的部分会被安全裁剪。 */
void OLED_ClearArea(uint8_t x, uint8_t y, uint8_t width, uint8_t height);

/** 设置或清除一个像素；超出范围的坐标会被忽略。 */
void OLED_DrawPoint(uint8_t x, uint8_t y, bool on);

/** 绘制任意方向直线。 */
void OLED_DrawLine(
    uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool on);

/** 绘制矩形；filled为true时填充内部。 */
void OLED_DrawRectangle(uint8_t x, uint8_t y,
    uint8_t width, uint8_t height, bool filled);

/** 绘制一个ASCII字符。非法字符会显示为问号。 */
void OLED_ShowChar(uint8_t x, uint8_t y, char character, OLED_Font font);

/** 绘制ASCII字符串，支持换行符\n；只修改RAM显存。 */
void OLED_ShowString(
    uint8_t x, uint8_t y, const char *string, OLED_Font font);

/**
 * @brief 显示无符号整数。
 * @param digits 为0时按实际位数显示；非0时不足位数在左侧补0。
 */
void OLED_ShowUInt(uint8_t x, uint8_t y,
    uint32_t value, uint8_t digits, OLED_Font font);

/**
 * 将发生变化的显存页发送到OLED。没有内容变化时不会产生I2C传输。
 */
bool OLED_Refresh(void);

/** 设置显示对比度，数值越低通常越省电。 */
bool OLED_SetContrast(uint8_t contrast);

/** 开启电荷泵和显示。 */
bool OLED_DisplayOn(void);

/** 关闭显示和电荷泵，RAM显存仍保留在MCU中。 */
bool OLED_DisplayOff(void);

/** 返回最近一次初始化或刷新是否成功。 */
bool OLED_IsReady(void);

#endif
