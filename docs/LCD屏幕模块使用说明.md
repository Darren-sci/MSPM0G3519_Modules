# MSPM0G3519 LCD屏幕模块使用说明

## 模块范围

本模块只包含5英寸LCD相关功能，没有迁移原示波器的ADC、波形界面、触摸、
按键、串口和App层。当前屏幕工作参数如下：

- 物理分辨率：480 x 854
- 显示方向：854 x 480横屏
- 接口：16位8080并口
- 像素格式：RGB565
- 逻辑电压：3.3V
- 当前只写屏幕，`RD`始终保持高电平

## 接线表

观察开发板正面时，让顶部调试接口朝上、底部两个按键朝下。

### 数据线

| LCD | G3519 | 开发板位置 |
| --- | --- | --- |
| D00 | PB0 | 左上长排针第3排右侧 |
| D01 | PB1 | 左上长排针第3排左侧 |
| D02 | PB2 | 左上长排针第4排左侧 |
| D03 | PB3 | 左上长排针第5排右侧 |
| D04 | PB4 | 左上长排针第5排左侧 |
| D05 | PB5 | 左上长排针第6排右侧 |
| D06 | PB6 | 左上长排针第13排左侧 |
| D07 | PB7 | 左上长排针第13排右侧 |
| D08 | PB8 | 左上长排针第14排左侧 |
| D09 | PB9 | 左上长排针第14排右侧 |
| D10 | PB10 | 左上长排针第15排左侧 |
| D11 | PB11 | 左上长排针第15排右侧 |
| D12 | PB12 | 左下排针第1排左侧 |
| D13 | PB13 | 左下排针第1排右侧 |
| D14 | PB14 | 左下排针第2排左侧 |
| D15 | PB15 | 左下排针第2排右侧 |

### 控制线和电源

| LCD | G3519 | 开发板位置 |
| --- | --- | --- |
| WR | PA8 | 左上长排针第6排左侧 |
| RD | PA9 | 左上长排针第7排右侧 |
| CS | PA12 | 左下排针第3排右侧 |
| RS/DC | PA13 | 左下排针第4排左侧 |
| RST | PA14 | 左下排针第4排右侧 |
| BL | PB16 | 左下排针第3排左侧；PA15保留给DAC0_OUT |
| VCC | 3V3 | 左下排针最后一排右侧 |
| GND | GND | 左下排针最后一排左侧 |

LCD的两个GND都应连接开发板GND。`D16-D23`、触摸、SPI和NC引脚暂时不接。

## 初始化

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

必须先执行 `SYSCFG_DL_init()`，再执行 `LCDPanel_init()`。屏幕初始化函数会
自动完成GPIO安全电平、硬件复位、寄存器配置、横屏、RGB565、退出休眠、
开显示、清黑屏和打开背光。

当前 `empty.c` 已经写好上电测试画面。烧录后应看到：

1. 顶部红、绿、蓝三个色条；
2. 一个白色矩形框；
3. 黄色文字 `MSPM0G3519 LCD READY`。

## 常用绘图函数

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

连续写入RGB565图片：

```c
LCD8080_setWindow(x, y, x + width - 1, y + height - 1);
LCD8080_writePixels(pixels, (uint32_t)width * height);
```

## ASCII字库

英文、数字和符号使用模块内置的ASCII 5 x 7点阵，可以整数倍放大：

```c
LCDText_drawAsciiString(30, 40, "Frequency: 1000 Hz",
    LCD_COLOR_WHITE, LCD_COLOR_BLACK,
    2,  /* 放大倍数 */
    0); /* 0=绘制背景，1=透明背景 */
```

## 中文软字库

中文使用保存在MCU Flash中的软件字模，不依赖W25Q64。字符串按UTF-8编写，
字模表使用Unicode码点查找。

16 x 16汉字每个需要32字节，数据按行排列、每行2字节、最高位对应左侧像素：

```c
static const uint8_t glyph_zhong[32] = {
    /* 在这里放“中”的16行点阵数据 */
};

static const LCDText_SoftGlyph chineseGlyphs[] = {
    {0x4E2D, 16, 16, glyph_zhong} /* Unicode U+4E2D：中 */
};

LCDText_drawUtf8(30, 100, "中文 ABC 123",
    chineseGlyphs,
    sizeof(chineseGlyphs) / sizeof(chineseGlyphs[0]),
    LCD_COLOR_YELLOW, LCD_COLOR_BLACK,
    1, 0);
```

最终界面用到哪些汉字，就把哪些汉字加入软字库，不必保存完整中文字库。找不到
字模时，驱动会显示一个16 x 16方框，便于发现遗漏字符。

## 文件结构

| 文件 | 作用 |
| --- | --- |
| `empty.syscfg` | 3519的22根LCD GPIO配置 |
| `BSP/board_pins.h` | 板级引脚封装 |
| `Drivers/lcd_8080.*` | 8080总线、窗口、像素和块写入 |
| `Drivers/lcd_panel.*` | 同款LCD控制器初始化序列 |
| `Graphics/lcd_graphics.*` | 线、矩形、圆等图形函数 |
| `Graphics/lcd_text.*` | ASCII和UTF-8中文软字库接口 |

## 故障检查顺序

背光亮但没有图像时，依次检查：

1. 屏幕3.3V供电和共地；
2. `RST`启动时是否先低后高；
3. `CS`是否为低，`RD`是否为高；
4. 清屏时`WR`是否持续产生脉冲；
5. `RS/DC`和`CS`是否接反；
6. D00-D15是否错位或倒序；
7. 杜邦线是否过长。若时序不稳定，可增加 `LCD8080_writeBus()` 中的
   `__NOP()` 数量。
