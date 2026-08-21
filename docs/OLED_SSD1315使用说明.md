# 0.96英寸 SSD1315 OLED 使用说明

## 1. 适用范围

本驱动用于四针 I²C OLED 模块，默认参数如下：

- 控制器：SSD1315，兼容常见 SSD1306 命令集
- 分辨率：128 × 64，单色
- 接口：I²C
- 7位地址：`0x3C`
- 总线速率：400 kHz
- 主控：MSPM0G3519，LQFP-100

驱动由以下文件组成：

- `Drivers/oled_ssd1315.h`：公开接口和分辨率、字体定义
- `Drivers/oled_ssd1315.c`：I²C传输、初始化、显存和绘图实现
- `empty.syscfg`：I²C0及PA10、PA11复用配置
- `empty.c`：启用宏、运行状态和更新事件示例

## 2. 引脚分配

| OLED引脚 | MSPM0G3519引脚 | 外设功能 | 说明 |
|---|---|---|---|
| GND | GND | — | 主控与OLED必须共地 |
| VCC | 3.3V | — | 不建议接5V |
| SCL | PA11 | I2C0_SCL / PINCM22 | I²C时钟 |
| SDA | PA10 | I2C0_SDA / PINCM21 | I²C数据 |

SCL和SDA均为开漏信号，必须上拉到3.3V。很多OLED模块板载4.7kΩ或
10kΩ上拉电阻；若模块没有上拉，应在主板上分别增加一个4.7kΩ电阻。

建议在OLED供电接口附近放置100nF和4.7µF～10µF去耦电容。

## 3. 占用资源

| 资源 | 占用情况 |
|---|---|
| GPIO复用 | PA10、PA11 |
| 硬件外设 | I2C0控制器模式 |
| DMA | 不使用 |
| 中断 | 不使用，当前采用带超时的轮询发送 |
| 定时器 | 不使用 |
| SRAM | 1024字节显存及少量状态变量 |
| 字库Flash | 一份5×7 ASCII点阵，约480字节 |

`OLED_Refresh()`采用脏页刷新。128×64屏幕分为8页，每页高8像素；只有内容
发生变化的页才会通过I²C发送，因此更新一行数字时不会重复发送完整1024字节。

## 4. 功能开关和状态机

`empty.c`中的总开关为：

```c
#define ENABLE_OLED    (1)
```

- 设为`1`：初始化OLED并运行显示状态机。
- 设为`0`：OLED初始化、绘图和刷新代码不在主函数中执行。

主函数示例使用两个概念：

1. `gOledState`记录OLED是否未初始化、正常或错误。
2. `gOledUpdateFlags`记录哪部分页面需要重画。

业务代码不应在ADC或定时器中断里直接刷新屏幕，只需要保存数据并置更新事件：

```c
gOledUpdateFlags |= MAIN_OLED_UPDATE_ADC1;
```

主循环最后调用`Main_processOLED()`，集中完成绘图和一次刷新。I²C无应答或超时
后状态会进入`MAIN_OLED_STATE_ERROR`，停止继续刷新，避免故障屏幕长期阻塞主循环。

当前示例在SSD1315屏幕上显示：

- 驱动名称和运行状态
- ADC1最近一个采样码
- ADC1 DMA缓冲区溢出计数

ADC1每250个1024点数据块提出一次OLED更新请求。需要改变更新速度时修改：

```c
#define OLED_UI_REFRESH_BLOCKS    (250U)
```

## 5. 最基本的函数使用

必须先完成SysConfig初始化，再初始化OLED：

```c
SYSCFG_DL_init();

if (OLED_Init()) {
    /* OLED_Init已经清除了物理屏幕。 */
}
```

显示一页内容：

```c
OLED_ClearBuffer();
OLED_ShowString(0U, 0U, "OLED OK", OLED_FONT_6X8);
OLED_ShowString(0U, 16U, "VALUE:", OLED_FONT_6X8);
OLED_ShowUInt(42U, 16U, 1234U, 0U, OLED_FONT_6X8);
OLED_DrawLine(0U, 30U, 127U, 30U, true);
OLED_Refresh();
```

这里的`OLED_ShowString()`、`OLED_ShowUInt()`和绘图函数只修改MCU中的RAM显存，
最后的`OLED_Refresh()`才发送I²C数据。应先画完一整批内容，再刷新一次。

## 6. 公开接口

### 初始化和刷新

```c
bool OLED_Init(void);
bool OLED_Refresh(void);
bool OLED_IsReady(void);
```

`OLED_Init()`必须在`SYSCFG_DL_init()`之后调用。它会等待模块上电稳定，发送
SSD1315初始化命令、清除屏幕并开启显示。屏幕未连接或地址不正确时返回`false`。

### 清屏

```c
void OLED_ClearBuffer(void);
bool OLED_ClearScreen(void);
void OLED_ClearArea(uint8_t x, uint8_t y,
    uint8_t width, uint8_t height);
```

- `OLED_ClearBuffer()`：只清RAM显存，适合重画页面前使用。
- `OLED_ClearScreen()`：清RAM后立即刷新物理屏幕。
- `OLED_ClearArea()`：清除指定矩形，适合更新位数可能缩短的动态数字。

### 文字

```c
void OLED_ShowChar(uint8_t x, uint8_t y,
    char character, OLED_Font font);
void OLED_ShowString(uint8_t x, uint8_t y,
    const char *string, OLED_Font font);
void OLED_ShowUInt(uint8_t x, uint8_t y,
    uint32_t value, uint8_t digits, OLED_Font font);
```

字体枚举：

| 字体 | 字符占用像素 |
|---|---:|
| `OLED_FONT_6X8` | 6 × 8 |
| `OLED_FONT_12X16` | 12 × 16 |
| `OLED_FONT_18X24` | 18 × 24 |

`OLED_ShowUInt()`的`digits`传0表示显示实际位数，非0表示左侧补0。例如：

```c
OLED_ShowUInt(0U, 0U, 25U, 4U, OLED_FONT_6X8);
```

显示结果为`0025`。

当前内置字库只支持ASCII。显示中文需要另外加入中文字模，不能直接把UTF-8中文
字符串交给`OLED_ShowString()`。

### 图形

```c
void OLED_DrawPoint(uint8_t x, uint8_t y, bool on);
void OLED_DrawLine(uint8_t x1, uint8_t y1,
    uint8_t x2, uint8_t y2, bool on);
void OLED_DrawRectangle(uint8_t x, uint8_t y,
    uint8_t width, uint8_t height, bool filled);
```

所有绘图函数都有屏幕边界保护。`OLED_DrawPoint()`和`OLED_DrawLine()`中的`on`
为`true`时点亮像素，为`false`时清除像素。

### 亮度和休眠

```c
bool OLED_SetContrast(uint8_t contrast);
bool OLED_DisplayOn(void);
bool OLED_DisplayOff(void);
```

默认对比度为`0x5F`。降低对比度通常能降低OLED功耗，例如：

```c
OLED_SetContrast(0x40U);
```

闲置时可以关闭显示和内部电荷泵：

```c
OLED_DisplayOff();
```

唤醒时：

```c
OLED_DisplayOn();
```

这些命令不会切断OLED模块VCC。若需要最低待机功耗，应另加负载开关控制VCC，
并在重新上电后再次调用`OLED_Init()`。

## 7. 修改页面内容

固定页面内容位于`empty.c`的`Main_processOLED()`，动态ADC内容位于
`Main_drawOLEDADC1Data()`。推荐按下面方式扩展：

1. 在更新标志中增加一个位，例如`MAIN_OLED_UPDATE_POWER`。
2. 测量任务只保存功率值并置该标志。
3. 在`Main_processOLED()`中根据标志清除并重画对应区域。
4. 所有区域处理完成后统一调用一次`OLED_Refresh()`。

不要在高速ADC中断、DMA中断或定时器中断中调用OLED函数。

## 8. 常见问题

### 初始化返回false

依次检查：

1. VCC是否为3.3V并且已经共地。
2. SDA是否连接PA10，SCL是否连接PA11。
3. SDA和SCL是否有上拉电阻。
4. 模块地址是否为`0x3C`；少数模块为`0x3D`，此时修改驱动中的
   `OLED_I2C_ADDRESS`。
5. SDA、SCL是否接反或被其他器件拉低。

### 屏幕方向相反

初始化数组中的`0xA1`和`0xC8`控制左右、上下方向。可以分别改为`0xA0`和
`0xC0`。

### 内容残留

动态数字由较多位变为较少位时，应先调用`OLED_ClearArea()`清除原区域，再显示
新数字，最后调用一次`OLED_Refresh()`。

### 屏幕太亮或功耗偏高

降低`OLED_SetContrast()`参数，减少全白区域，并避免在内容不变时主动刷新。
