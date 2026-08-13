# 按键 GPIO 引脚规划

## 配置概览

本工程使用 MSPM0G3519 的 LQFP-100（PZ）封装。12 个按键统一配置在 `GPIOC` 的 `KEYS` Pin Group 中，全部为带内部上拉的数字输入。

- 按键未按下：读取为高电平 `1`
- 按键按下：读取为低电平 `0`
- 硬件连接：GPIO 与 GND 之间连接按键
- 建议软件消抖时间：10～20 ms

## 按键引脚与复用价值

“复用价值”表示该引脚除 GPIO 外可承担的外设功能。优先选择纯 GPIO；必须使用带复用功能的引脚时，优先牺牲较次要或已有替代引脚的功能。

| 按键 | GPIO | PZ-100 物理脚 | 主要复用功能 | 复用价值 | 选择说明 |
|---|---:|---:|---|---|---|
| KEY1 | PC12 | 10 | 无 | 最低（纯 GPIO） | 优先用于按键 |
| KEY2 | PC15 | 11 | 无 | 最低（纯 GPIO） | 优先用于按键 |
| KEY3 | PC16 | 35 | 无 | 最低（纯 GPIO） | 优先用于按键 |
| KEY4 | PC18 | 38 | 无 | 最低（纯 GPIO） | 优先用于按键 |
| KEY5 | PC20 | 58 | 无 | 最低（纯 GPIO） | 优先用于按键 |
| KEY6 | PC23 | 61 | 无 | 最低（纯 GPIO） | 优先用于按键 |
| KEY7 | PC24 | 62 | 无 | 最低（纯 GPIO） | 优先用于按键 |
| KEY8 | PC17 | 36 | TIMG14_C2 输入 | 低 | 仅占用次要定时器输入功能 |
| KEY9 | PC19 | 39 | TIMG9_C1 | 低 | 仅占用定时器通道，其他 GPIO 上有替代映射 |
| KEY10 | PC4 | 67 | UART3_CTS、SPI1_CS2、TIMA_FAL2、TIMA0_C1、TIMG14_C2 | 中 | 不占 UART3 的基本 TX/RX；流控和片选可用普通 GPIO 替代 |
| KEY11 | PC5 | 68 | UART3_RTS、SPI1_CS3、TIMG8_IDX、TIMA0_C1N、TIMG14_C3 | 中 | 不占 UART3 的基本 TX/RX；流控和片选可用普通 GPIO 替代 |
| KEY12 | PC8 | 80 | UART3_CTS、SPI1_CS2、TIMA0_C1 | 中 | 不占 UART3 的基本 TX/RX；功能存在其他映射 |

## 特意保留的成组外设引脚

| 保留引脚 | 可用外设 | 保留原因 |
|---|---|---|
| PC0 / PC1 | UART1 TX / RX | 保留完整串口对 |
| PC2 / PC3 | I2C2 SCL / SDA | 保留完整 I2C 总线 |
| PC6 / PC7 | UART3 TX / RX | 保留 UART3 基本通信功能 |
| PC10 / PC11 | UART6 RX / TX | 保留完整串口对 |
| PC13 / PC14 | SPI2 PICO / SCK | 避免提前占用 SPI2 关键线 |
| PC21 / PC22 | CAN1 TX / RX | 保留完整 CAN1 接口 |
| PC26 / PC27 | CAN1 TX / RX | 保留 CAN1 的另一组映射 |
| PC28 / PC29 | UART5 RX / TX | 保留完整串口对 |

## SysConfig 设置

`KEYS` 中每个引脚均使用以下设置：

```text
Direction         = Input
Internal Resistor = Pull Up
```

当前按键掩码为：

```c
#define KEYS_ALL_MASK (KEYS_KEY1_PIN  | KEYS_KEY2_PIN  | \
                       KEYS_KEY3_PIN  | KEYS_KEY4_PIN  | \
                       KEYS_KEY5_PIN  | KEYS_KEY6_PIN  | \
                       KEYS_KEY7_PIN  | KEYS_KEY8_PIN  | \
                       KEYS_KEY9_PIN  | KEYS_KEY10_PIN | \
                       KEYS_KEY11_PIN | KEYS_KEY12_PIN)
```

一次读取全部按键：

```c
uint32_t key_levels = DL_GPIO_readPins(KEYS_PORT, KEYS_ALL_MASK);

/* 内部上拉、按键接地，因此按下状态需要取反并限制在按键掩码内。 */
uint32_t keys_pressed = (~key_levels) & KEYS_ALL_MASK;
```

## GPIO 常用设置与函数

### SysConfig 常用设置

| 设置项 | 常见取值 | 作用 |
|---|---|---|
| Name | `KEY1`、`LED1` | 决定生成宏中的引脚名称 |
| Direction | `Input` / `Output` | 配置为输入或输出 |
| Assigned Port | `PORTA` / `PORTB` / `PORTC` | 选择 GPIO 端口 |
| Assigned Pin | `0`～`31` | 选择端口中的位编号，例如 `12` 表示 PC12 |
| Internal Resistor | `None` / `Pull Up` / `Pull Down` | 设置内部无上下拉、上拉或下拉；主要用于输入 |
| Initial Value | `Set` / `Cleared` | 输出使能时的初始高/低电平 |
| Invert | `Enabled` / `Disabled` | 是否在硬件中反转输入或输出逻辑 |
| Input Filter | `Enabled` / `Disabled` | 输入滤波；是否可用及具体效果以芯片配置为准 |
| Hysteresis | `Enabled` / `Disabled` | 输入迟滞，可提高慢沿或有噪声信号的稳定性 |
| Fast-Wake / Wakeup | `Enabled` / `Disabled` | 是否允许该输入参与低功耗唤醒 |
| Interrupts/Events | 边沿、极性等 | 配置 GPIO 中断或事件；普通轮询按键无需启用 |

工程启动时调用一次 `SYSCFG_DL_init()`，SysConfig 生成的代码就会完成 GPIO 供电、IOMUX、方向、上下拉和初始值等初始化。通常不需要在主程序中再次手动初始化。

### SysConfig 自动生成的宏

生成名称的基本格式为：

```text
<组名>_PORT
<组名>_<引脚名>_PIN
<组名>_<引脚名>_IOMUX
```

例如组名为 `KEYS`、引脚名为 `KEY1`、物理配置为 PC12 时：

```c
#define KEYS_PORT       (GPIOC)
#define KEYS_KEY1_PIN   (DL_GPIO_PIN_12)
#define KEYS_KEY1_IOMUX (IOMUX_PINCM61)
```

- `PORT` 宏表示 GPIO 外设端口，传给 GPIO 函数的第一个参数。
- `PIN` 宏是位掩码，传给 GPIO 函数的第二个参数。
- `IOMUX` 宏表示物理引脚配置寄存器，主要供初始化函数使用。

应用代码应优先使用这些宏，不要直接写死 `GPIOC` 或 `DL_GPIO_PIN_12`。以后只在 SysConfig 中换脚，应用代码通常无需修改。

### 读取输入电平

```c
uint32_t DL_GPIO_readPins(GPIO_Regs *gpio, uint32_t pins);
```

| 参数/返回值 | 含义 |
|---|---|
| `gpio` | GPIO 端口，例如 `KEYS_PORT` |
| `pins` | 要读取的一个或多个引脚掩码；多个引脚用按位或 `|` 连接 |
| 返回值 | 被选引脚中当前为高电平的位掩码；低电平对应位为 `0` |

读取单个引脚：

```c
uint32_t level = DL_GPIO_readPins(KEYS_PORT, KEYS_KEY1_PIN);

if (level != 0U) {
    /* KEY1 当前为高电平。 */
} else {
    /* KEY1 当前为低电平。 */
}
```

注意返回值是位掩码，不保证等于数字 `1`。例如 PC12 为高电平时，返回的是 `DL_GPIO_PIN_12`，所以应与 `0U` 比较。

本工程的按键使用内部上拉并接地，因此判断按下应写为：

```c
bool pressed =
    (DL_GPIO_readPins(KEYS_PORT, KEYS_KEY1_PIN) == 0U);
```

### 输出置高

```c
void DL_GPIO_setPins(GPIO_Regs *gpio, uint32_t pins);
```

| 参数/返回值 | 含义 |
|---|---|
| `gpio` | GPIO 端口 |
| `pins` | 需要置为高电平的引脚掩码 |
| 返回值 | 无 |

```c
DL_GPIO_setPins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_1_PIN);
```

该函数把指定输出脚置为逻辑高电平。外部器件是否因此“打开”，取决于它是高电平有效还是低电平有效。

### 输出置低

```c
void DL_GPIO_clearPins(GPIO_Regs *gpio, uint32_t pins);
```

| 参数/返回值 | 含义 |
|---|---|
| `gpio` | GPIO 端口 |
| `pins` | 需要置为低电平的引脚掩码 |
| 返回值 | 无 |

```c
DL_GPIO_clearPins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_1_PIN);
```

### 翻转输出电平

```c
void DL_GPIO_togglePins(GPIO_Regs *gpio, uint32_t pins);
```

| 参数/返回值 | 含义 |
|---|---|
| `gpio` | GPIO 端口 |
| `pins` | 需要翻转的引脚掩码 |
| 返回值 | 无 |

高电平会变低，低电平会变高，适合做状态灯或方波测试：

```c
DL_GPIO_togglePins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_1_PIN);
```

### 按掩码批量写入

```c
void DL_GPIO_writePinsVal(
    GPIO_Regs *gpio, uint32_t pinsMask, uint32_t pinsVal);
```

| 参数/返回值 | 含义 |
|---|---|
| `gpio` | GPIO 端口 |
| `pinsMask` | 允许修改的引脚掩码 |
| `pinsVal` | 要写入的位值；掩码内为 `1` 的脚置高，为 `0` 的脚置低 |
| 返回值 | 无 |

例如只修改 PB0～PB3，并输出二进制 `0101`：

```c
uint32_t mask = DL_GPIO_PIN_0 | DL_GPIO_PIN_1 |
                DL_GPIO_PIN_2 | DL_GPIO_PIN_3;

DL_GPIO_writePinsVal(GPIOB, mask, 0x05U);
```

`pinsMask` 之外的输出位保持不变。本工程 LCD 的 16 位数据总线也使用了这一函数。

### 整端口输出写入

```c
void DL_GPIO_writePins(GPIO_Regs *gpio, uint32_t pins);
```

| 参数/返回值 | 含义 |
|---|---|
| `gpio` | GPIO 端口 |
| `pins` | 整个端口的输出位值 |
| 返回值 | 无 |

该函数会按照 `pins` 的每一位更新端口中已使能的输出，因此可能同时影响同一端口上的多个设备。一般业务代码更推荐使用 `setPins()`、`clearPins()` 或带明确掩码的 `writePinsVal()`。

### 动态启用或关闭输出驱动

```c
void DL_GPIO_enableOutput(GPIO_Regs *gpio, uint32_t pins);
void DL_GPIO_disableOutput(GPIO_Regs *gpio, uint32_t pins);
```

| 函数 | 作用 | 返回值 |
|---|---|---|
| `DL_GPIO_enableOutput()` | 启用指定引脚的输出驱动 | 无 |
| `DL_GPIO_disableOutput()` | 关闭指定引脚的输出驱动，使其不再主动驱动高低电平 | 无 |

普通固定输入/输出通常由 SysConfig 自动处理，不需要在主循环中调用这两个函数。它们主要用于运行时需要切换引脚驱动状态的场景。

### 多引脚掩码写法

GPIO 函数的 `pins` 参数是位掩码，因此可以一次操作同一端口中的多个引脚：

```c
uint32_t two_keys = KEYS_KEY1_PIN | KEYS_KEY2_PIN;
uint32_t levels   = DL_GPIO_readPins(KEYS_PORT, two_keys);
```

不能用 `+` 代替 `|`。另外，一次调用只能操作同一个 GPIO 端口；GPIOA 和 GPIOC 的引脚需要分开调用。

后续新增 UART、I2C、SPI、CAN 或定时器模块时，应先在 SysConfig 中检查 PinMux 冲突，并同步更新本表。
