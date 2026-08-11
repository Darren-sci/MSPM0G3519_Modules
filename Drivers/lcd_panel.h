#ifndef DRIVERS_LCD_PANEL_H_
#define DRIVERS_LCD_PANEL_H_

#include <stdint.h>

/**
 * @brief 初始化LCD控制器并打开显示。
 *
 * 该函数完成8080总线初始化、硬件复位、电源和Gamma参数配置、横屏设置、
 * RGB565设置、退出休眠、清黑屏和打开背光。调用前必须先执行
 * SYSCFG_DL_init()。初始化后的逻辑分辨率为854x480。
 */
void LCDPanel_init(void);

/**
 * @brief 打开或关闭LCD显示及背光。
 * @param enabled 非0表示开启显示并打开背光，0表示关闭显示并关闭背光。
 *
 * 关闭显示不会主动清除LCD显存内容。
 */
void LCDPanel_displayOn(uint8_t enabled);

#endif
