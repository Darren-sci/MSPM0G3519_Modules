# MSPM0G3519 DriverLib 常用函数指南

> 适用芯片：MSPM0G3519（MSPM0GX51X）  
> 对应 SDK：MSPM0 SDK 2.11.00.07  
> 工程参考：`D:\TI\Project\G3519Modules`  
> 用途：比赛期间离线查阅，也可以作为 Qwen Coder 的项目上下文。

## 1. 先理解四层结构

```text
main.c / 业务模块
        ↓ 优先调用
项目封装驱动（ADC1Fast_*、ADCMulti_*、DACOutput_*、PWMOutput_* 等）
        ↓ 必要时调用
TI DriverLib（DL_GPIO_*、DL_ADC12_*、DL_DMA_* 等）
        ↓
MSPM0G3519 寄存器和硬件
```

SysConfig 文件之间的关系：

```text
empty.syscfg
    ↓ SysConfig 生成
Debug/ti_msp_dl_config.h   外设实例、引脚、通道和中断名称
Debug/ti_msp_dl_config.c   时钟、引脚和外设初始化代码
    ↓ 调用
source/ti/driverlib        真正的 DL_ 底层库
```

基本规则：

1. `main()` 开头先调用一次 `SYSCFG_DL_init()`。
2. 通常不要手改 `ti_msp_dl_config.c/.h`，重新生成时会被覆盖。
3. 初始化参数优先在 `.syscfg` 中修改。
4. 业务代码优先使用项目已经封装好的驱动，不要重复搭建 ADC+DMA 等复杂链路。
5. 直接调用 `DL_` 前，必须在对应的 `dl_*.h` 或官方例程中确认函数和枚举。

## 2. 当前工程的硬件配置

当前 `G3519Modules` 的 `ti_msp_dl_config.h/.c` 表明：

| 功能 | SysConfig 名称 | 硬件实例/引脚 | 说明 |
|---|---|---|---|
| CPU 时钟 | `CPUCLK_FREQ` | 32 MHz | 延时、定时器计算的基础 |
| 双通道 PWM | `PWM_0_INST` | TIMA0，PA0/PA1 | 固定 PWM 和 SPWM 共用 |
| ADC0 采样定时器 | `ADC_SAMPLE_TIMER_INST` | TIMG0 | 通过事件触发 ADC0 |
| OLED | `OLED_I2C_INST` | I2C0，PA10/PA11 | SSD1315，控制器发送模式 |
| ADC0 | `ADC_CAPTURE_INST` | ADC0，4 通道 | 定时触发、序列采样、FIFO、DMA0 |
| ADC1 | `ADC1_FAST_INST` | ADC1，单通道 | 连续高速采样、FIFO、DMA1 |
| DAC | `DAC0` | PA15 | 直流输出或 FIFO+DMA2 波形 |
| LCD | `LCD_DATA_*` / `LCD_CTRL_*` | 16 位 GPIO 并口 | 8080 总线 |
| 按键 | `KEYS_*` | 12 个 GPIO 输入 | 内部上拉，低电平按下 |

初始化顺序由 `SYSCFG_DL_init()` 统一完成：GPIO、系统时钟、TIMA0、TIMG0、I2C0、ADC0、ADC1、DMA 和 DAC0。

## 3. GPIO

对应资料：

```text
driverlib/dl_gpio.h
driverlib/mspm0gx51x_api_guide/
```

### 3.1 常用运行期函数

```c
DL_GPIO_setPins(port, pinMask);              // 指定引脚置 1
DL_GPIO_clearPins(port, pinMask);            // 指定引脚清 0
DL_GPIO_togglePins(port, pinMask);           // 指定引脚翻转
DL_GPIO_readPins(port, pinMask);             // 读取指定引脚，返回值仍在原位
DL_GPIO_writePinsVal(port, pinMask, value);  // 按掩码一次写多个引脚
```

`pinMask` 可以用按位或同时选择多个引脚：

```c
DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_0 | DL_GPIO_PIN_1);
```

### 3.2 LED 输出示例

GPIO 方向和引脚复用已经由 SysConfig 配好，业务代码只改变电平：

```c
DL_GPIO_setPins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_1_PIN);
DL_GPIO_clearPins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_1_PIN);
DL_GPIO_togglePins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_1_PIN);
```

高低电平是否代表点亮取决于 LED 的接法。

### 3.3 按键输入示例

当前工程按键为内部上拉、按下接地，所以低电平表示按下：

```c
bool pressed =
    (DL_GPIO_readPins(KEYS_PORT, KEYS_KEY1_PIN) == 0U);
```

工程已经封装消抖和单击检测，实际业务优先使用：

```c
if (Key_wasClicked(KEY_1)) {
    /* KEY1 单击事件 */
}
```

### 3.4 LCD 16 位并口

工程用低 16 位 GPIO 一次写出一个 RGB565 数据：

```c
DL_GPIO_writePinsVal(
    BOARD_LCD_DATA_PORT,
    BOARD_LCD_DATA_MASK,
    (uint32_t)value);

DL_GPIO_clearPins(BOARD_LCD_CTRL_PORT, BOARD_LCD_WR_PIN);
__NOP();
__NOP();
DL_GPIO_setPins(BOARD_LCD_CTRL_PORT, BOARD_LCD_WR_PIN);
```

`DL_GPIO_writePinsVal()` 的 `value` 必须与引脚位位置对齐，不能把任意未对齐数值直接写给零散引脚。

## 4. 延时、CPU 和中断控制

### 4.1 周期延时

工程使用：

```c
delay_cycles(cycles);
```

毫秒延时的常见换算：

```c
delay_cycles((CPUCLK_FREQ / 1000U) * milliseconds);
```

当前 `CPUCLK_FREQ` 为 32 MHz。忙等待期间 CPU 不能处理主循环工作，不适合长时间延时或高速采集路径。

### 4.2 NVIC 常用函数

```c
NVIC_ClearPendingIRQ(IRQn);  // 清除 NVIC 中尚未处理的挂起状态
NVIC_EnableIRQ(IRQn);        // 允许该外设中断进入 CPU
NVIC_DisableIRQ(IRQn);       // 禁止该外设中断进入 CPU
```

SysConfig 会生成中断号宏，例如：

```c
ADC_CAPTURE_INST_INT_IRQN
ADC1_FAST_INST_INT_IRQN
PWM_0_INST_INT_IRQN
DAC12_INT_IRQN
```

需要保护很短的共享状态时：

```c
uint32_t primask = __get_PRIMASK();
__disable_irq();

/* 修改 ISR 和主循环共享的状态 */

if (primask == 0U) {
    __enable_irq();
}
```

不要在关中断区执行 LCD 刷新、FFT、I2C 传输或长循环。

## 5. 定时器和 PWM

对应资料：

```text
driverlib/dl_timer.h
driverlib/dl_timera.h
driverlib/dl_timerg.h
```

### 5.1 类型关系

- `DL_TimerA_*`：TIMA 专用接口，当前用于 TIMA0 PWM。
- `DL_TimerG_*`：TIMG 专用接口，当前用于 TIMG0 ADC 采样触发。
- `DL_Timer_*`：部分 TimerA/TimerG 共用操作。

不要把 `DL_TimerA_*` 用在 TIMG 实例上，也不要仅凭名称猜测接口是否通用。

### 5.2 启停和计数值

```c
DL_TimerA_startCounter(PWM_0_INST);
DL_TimerA_stopCounter(PWM_0_INST);

DL_TimerG_startCounter(ADC_SAMPLE_TIMER_INST);
DL_TimerG_stopCounter(ADC_SAMPLE_TIMER_INST);

DL_Timer_setLoadValue(ADC_SAMPLE_TIMER_INST, loadValue);
DL_Timer_setTimerCount(ADC_SAMPLE_TIMER_INST, currentValue);
```

对于当前向下计数的采样定时器，常用计算是：

```c
uint32_t timerTicks = CPUCLK_FREQ / targetRateHz;
DL_Timer_setLoadValue(ADC_SAMPLE_TIMER_INST, timerTicks - 1U);
DL_Timer_setTimerCount(ADC_SAMPLE_TIMER_INST, timerTicks - 1U);
```

实际频率是整数分频结果：

```text
actualRate = CPUCLK_FREQ / timerTicks
```

### 5.3 修改 PWM 占空比

```c
uint32_t period = DL_TimerA_getLoadValue(PWM_0_INST);
DL_TimerA_setCaptureCompareValue(
    PWM_0_INST,
    compareValue,
    GPIO_PWM_0_C0_IDX);
```

当前工程已经处理了计数模式和极性带来的占空比换算，优先使用：

```c
PWMOutput_setChannel0Duty(300U);  // PA0，30.0%
PWMOutput_setChannel1Duty(600U);  // PA1，60.0%
DL_TimerA_startCounter(PWM_0_INST);
```

这里占空比单位是千分比：`0`～`1000` 对应 `0%`～`100%`。

### 5.4 SPWM

当前工程用 TIMA0 的零事件中断更新比较值：

```c
SPWM_init();
SPWM_set(SPWM_CHANNEL_0, 1000U, 800U, 0U);
SPWM_start(SPWM_CHANNEL_0);
```

参数依次是：通道、正弦频率 Hz、幅度千分比、初相位角度。

ISR 中的核心结构：

```c
void TIMA0_IRQHandler(void)
{
    if (DL_TimerA_getPendingInterrupt(PWM_0_INST) ==
        DL_TIMER_IIDX_ZERO) {
        /* 更新两个通道的捕获比较值 */
    }
}
```

注意：

- 固定 PWM 和 SPWM 共用 TIMA0，同一通道不能同时由两种模式控制。
- ISR 中只做相位累加和比较值更新，不做浮点重计算、显示或打印。
- 频率和占空比的精确关系取决于 SysConfig 中的计数模式、预分频和 Load 值。

## 6. ADC12

对应资料：

```text
driverlib/dl_adc12.h
```

### 6.1 常用运行期函数

```c
DL_ADC12_enableConversions(adc);       // 允许 ADC 转换
DL_ADC12_disableConversions(adc);      // 禁止 ADC 转换
DL_ADC12_startConversion(adc);         // 软件启动转换
DL_ADC12_stopConversion(adc);          // 停止软件触发的转换流程

DL_ADC12_enableFIFO(adc);
DL_ADC12_disableFIFO(adc);
DL_ADC12_enableDMA(adc);
DL_ADC12_disableDMA(adc);

DL_ADC12_getMemResultAddress(adc, memIndex); // 某 MEM 结果寄存器地址
DL_ADC12_getFIFOAddress(adc);                // ADC FIFO 数据地址

DL_ADC12_clearInterruptStatus(adc, mask);
DL_ADC12_getPendingInterrupt(adc);           // 取得当前 IIDX 中断原因
```

`enableConversions()` 是允许转换，`startConversion()` 是发出软件启动动作，两者不是同一件事。具体是否需要 `startConversion()` 取决于触发方式：

- ADC1 当前是软件启动后连续转换。
- ADC0 当前由 TIMG0 事件触发，不需要每帧软件调用 `startConversion()`。

### 6.2 ADC0：四通道定时采样

当前配置：

```text
TIMG0 零事件 → ADC0 序列转换 MEM0～MEM3 → FIFO → DMA0 → 双缓冲区
```

项目业务代码优先使用：

```c
ADCMulti_init();
bool started = ADCMulti_start(10000U); // 每通道 10 kframe/s

const ADCMulti_Frame *frames;
uint16_t frameCount;

if (started && ADCMulti_getReadyBuffer(&frames, &frameCount)) {
    /* 必须在 release 前读取或复制 frames */
    ADCMulti_releaseBuffer(frames);
}
```

每一帧的固定顺序：

```c
typedef struct {
    uint16_t vin;
    uint16_t iin;
    uint16_t vout;
    uint16_t iout;
} ADCMulti_Frame;
```

### 6.3 ADC1：单通道高速连续采样

当前配置：

```text
ADC1 连续转换 → FIFO → DMA1 → 两个 1024 点缓冲区
```

项目业务代码优先使用：

```c
ADC1Fast_init();
bool started = ADC1Fast_start();

const uint16_t *samples;
uint16_t sampleCount;

if (started && ADC1Fast_getReadyBuffer(&samples, &sampleCount)) {
    /* 在这里处理或复制数据 */
    ADC1Fast_releaseBuffer(samples);
}
```

重要：

- `samples` 只在取得缓冲区到释放缓冲区之间有效。
- FFT、LCD 刷新等较慢操作最好先复制快照，再尽快释放 DMA 缓冲区。
- 当前工程使用约 `3988513 Hz` 的实测标定值进行频谱分析，这属于应用层标定，不应当当成所有配置下固定的 ADC 采样率。

### 6.4 ADC DMA 完成中断

```c
void ADC1_FAST_INST_IRQHandler(void)
{
    if (DL_ADC12_getPendingInterrupt(ADC1_FAST_INST) ==
        DL_ADC12_IIDX_DMA_DONE) {
        /* 标记当前缓冲区完成，切换并重新装载下一个 DMA 缓冲区 */
    }
}
```

ISR 应只完成状态切换、计数和下一块 DMA 装载。不要在 ISR 中做 FFT、LCD、OLED 或长时间等待。

## 7. DMA

对应资料：

```text
driverlib/dl_dma.h
```

### 7.1 常用函数

```c
DL_DMA_disableChannel(DMA, channel);
DL_DMA_setSrcAddr(DMA, channel, sourceAddress);
DL_DMA_setDestAddr(DMA, channel, destinationAddress);
DL_DMA_setTransferSize(DMA, channel, transferCount);
DL_DMA_enableChannel(DMA, channel);
DL_DMA_isChannelEnabled(DMA, channel);
```

当前工程通道分配：

```text
DMA0：ADC0 四通道序列采集
DMA1：ADC1 高速单通道采集
DMA2：DAC 波形表循环输出
```

### 7.2 安全的重新装载顺序

```c
DL_DMA_disableChannel(DMA, channel);
DL_DMA_setSrcAddr(DMA, channel, sourceAddress);
DL_DMA_setDestAddr(DMA, channel, destinationAddress);
DL_DMA_setTransferSize(DMA, channel, transferCount);
DL_DMA_enableChannel(DMA, channel);
```

ADC FIFO 到内存的示意：

```c
DL_DMA_disableChannel(DMA, ADC_DMA_CHAN_ID);
DL_DMA_setSrcAddr(
    DMA,
    ADC_DMA_CHAN_ID,
    DL_ADC12_getFIFOAddress(ADC_CAPTURE_INST));
DL_DMA_setDestAddr(
    DMA,
    ADC_DMA_CHAN_ID,
    (uint32_t)&buffer[0]);
DL_DMA_setTransferSize(DMA, ADC_DMA_CHAN_ID, wordCount);
DL_DMA_enableChannel(DMA, ADC_DMA_CHAN_ID);
DL_ADC12_enableDMA(ADC_CAPTURE_INST);
```

注意：

- 修改地址和数量前先关闭通道。
- 传输数量的单位由 DMA 配置的数据宽度决定，不一定等于字节数。
- 缓冲区必须在 DMA 完成前始终有效；不能指向已经退出函数的局部数组。
- CPU 与 ISR/DMA 共享的状态需要 `volatile` 或短临界区，但 `volatile` 本身不保证复合操作原子性。
- 当前 ADC 驱动每完成一个 DMA 块后会重新使能 ADC DMA 请求并装载下一块。

## 8. DAC12

对应资料：

```text
driverlib/dl_dac12.h
```

### 8.1 直接输出直流

底层调用：

```c
DL_DAC12_enable(DAC0);
DL_DAC12_output12(DAC0, code); // 0～4095
```

当前工程优先使用：

```c
DACOutput_init();
DACOutput_setCode(3072U);
```

或者按参考电压换算：

```c
DACOutput_setMilliVolts(1650U, 3300U);
```

这里的参考电压最好填写实测 VDDA。

### 8.2 DMA 波形输出

当前链路：

```text
波形表 → DMA2 → DAC FIFO → DAC 内部采样发生器 → PA15
```

项目封装示例：

```c
static const uint16_t wave[] = {
    512U, 512U, 512U, 512U,
    3584U, 3584U, 3584U, 3584U
};

DACOutput_startWaveform(
    wave,
    (uint16_t)(sizeof(wave) / sizeof(wave[0])),
    DAC_OUTPUT_RATE_8_KHZ);
```

底层相关函数包括：

```c
DL_DAC12_disableFIFO(DAC0);
DL_DAC12_enableFIFO(DAC0);
DL_DAC12_setFIFOTriggerSource(
    DAC0, DL_DAC12_FIFO_TRIGGER_SAMPLETIMER);
DL_DAC12_setFIFOThreshold(
    DAC0, DL_DAC12_FIFO_THRESHOLD_TWO_QTRS_EMPTY);
DL_DAC12_setSampleRate(DAC0, driverlibRate);
DL_DAC12_enableDMATrigger(DAC0);
DL_DAC12_enableSampleTimeGenerator(DAC0);
```

停止时按工程封装执行：

```c
DACOutput_stopWaveform();
```

波形表必须是静态、全局或其他长期有效内存，不能传入即将失效的局部数组。

## 9. I2C 与 OLED

对应资料：

```text
driverlib/dl_i2c.h
Drivers/oled_ssd1315.c
```

当前工程使用 I2C0 控制器发送模式。初始化由 SysConfig 完成，业务代码直接使用 OLED 封装：

```c
if (OLED_Init()) {
    OLED_ShowString(0U, 0U, "HELLO", OLED_FONT_6X8);
    OLED_ShowUInt(0U, 16U, 1234U, 0U, OLED_FONT_6X8);
    OLED_Refresh();
}
```

底层发送流程：

```c
DL_I2C_flushControllerTXFIFO(OLED_I2C_INST);
DL_I2C_transmitControllerData(OLED_I2C_INST, controlByte);
DL_I2C_fillControllerTXFIFO(OLED_I2C_INST, data, initialCount);

DL_I2C_startControllerTransfer(
    OLED_I2C_INST,
    slaveAddress,
    DL_I2C_CONTROLLER_DIRECTION_TX,
    totalLength);
```

轮询状态：

```c
uint32_t status = DL_I2C_getControllerStatus(OLED_I2C_INST);

if ((status & DL_I2C_CONTROLLER_STATUS_ERROR) != 0U) {
    DL_I2C_resetControllerTransfer(OLED_I2C_INST);
    DL_I2C_flushControllerTXFIFO(OLED_I2C_INST);
}
```

常用状态：

```c
DL_I2C_CONTROLLER_STATUS_IDLE
DL_I2C_CONTROLLER_STATUS_BUSY
DL_I2C_CONTROLLER_STATUS_ERROR
```

必须设置超时，不能无限等待 `BUSY` 清除。当前 OLED 驱动还针对 MSPM0 I2C ERR-13 在启动传输后加入了至少 3 个 I2C 功能时钟的等待。

## 10. 当前工程优先使用的封装接口

比赛时如果只是实现功能，优先把下面这些头文件和调用方式交给模型，而不是让模型直接重写底层链路。

### ADC0 四通道

```c
ADCMulti_init();
ADCMulti_start(frameRateHz);
ADCMulti_getReadyBuffer(&frames, &frameCount);
ADCMulti_releaseBuffer(frames);
ADCMulti_stop();
```

### ADC1 高速采样

```c
ADC1Fast_init();
ADC1Fast_start();
ADC1Fast_getReadyBuffer(&samples, &sampleCount);
ADC1Fast_releaseBuffer(samples);
ADC1Fast_stop();
```

### DAC

```c
DACOutput_init();
DACOutput_setCode(code);
DACOutput_setMilliVolts(mV, referenceMV);
DACOutput_startWaveform(table, length, sampleRate);
DACOutput_stopWaveform();
```

### 固定 PWM

```c
PWMOutput_setChannel0Duty(dutyPermille);
PWMOutput_setChannel1Duty(dutyPermille);
DL_TimerA_startCounter(PWM_0_INST);
```

### SPWM

```c
SPWM_init();
SPWM_set(channel, frequencyHz, amplitudePermille, phaseDegree);
SPWM_start(channel);
SPWM_stop(channel);
```

### 按键

```c
if (Key_wasClicked(KEY_1)) {
    /* 单击事件 */
}
```

### OLED

```c
OLED_Init();
OLED_ClearBuffer();
OLED_ShowString(x, y, text, font);
OLED_Refresh();
```

### LCD

```c
LCDPanel_init();
LCD8080_clear(LCD_COLOR_BLACK);
LCD8080_fillRect(x, y, width, height, color);
LCD8080_drawPixel(x, y, color);
```

## 11. 当前工程暂未使用的外设

工程目前没有直接使用 UART、SPI、CAN-FD、RTC、运放或比较器。需要这些功能时，先从以下位置找同芯片官方例程：

```text
LP_MSPM0G3519/
driverlib/mspm0gx51x_api_guide/
```

相关头文件：

```text
UART：driverlib/dl_uart_main.h、dl_uart.h
SPI： driverlib/dl_spi.h
CAN： driverlib/dl_mcan.h
RTC： driverlib/dl_rtc*.h
OPA： driverlib/dl_opa.h
COMP：driverlib/dl_comp.h
```

让模型添加新外设时，至少同时提供：

1. 当前 `empty.syscfg`。
2. 当前 `ti_msp_dl_config.h`。
3. 对应 `dl_*.h`。
4. 一个 `LP_MSPM0G3519` 官方例程。
5. 完整编译错误。

## 12. 常见错误检查表

### 找不到宏或实例

例如模型写了 `UART_0_INST`，但 `ti_msp_dl_config.h` 中没有：说明 SysConfig 尚未创建这个实例，不能靠业务代码凭空使用该名称。

### 函数名看起来合理但编译不存在

直接在对应头文件搜索完整函数名。不同 UART、Timer 或芯片系列的 API 不能混用。

### ADC 没数据

依次检查：

1. `SYSCFG_DL_init()` 是否调用。
2. ADC 是否允许转换。
3. 软件触发或事件触发是否真正发生。
4. DMA 源地址是否选 FIFO/MEM 结果地址。
5. DMA 通道是否使能。
6. ADC DMA 请求是否使能。
7. 中断号和 ISR 名称是否使用 SysConfig 生成宏。

### DMA 只运行一次

块传输完成后通常需要重新装载目标地址、数量并再次使能通道；当前 ADC 驱动还需要重新使能 ADC DMA 请求。

### PWM 不输出

检查：

1. PA0/PA1 是否仍配置为 TIMA0 CCP 功能。
2. 比较通道索引是否使用 `GPIO_PWM_0_C0_IDX/C1_IDX`。
3. 是否调用 `DL_TimerA_startCounter(PWM_0_INST)`。
4. 是否同时启用了固定 PWM 和同通道 SPWM。

### I2C 卡死

所有 `BUSY`、FIFO 和完成轮询都必须带超时；错误后复位传输并清 FIFO。

### 修改生成文件后又恢复原样

`ti_msp_dl_config.c/.h` 是生成文件。应修改 `.syscfg` 或在 `SYSCFG_DL_init()` 之后写额外运行期配置。

## 13. 给 Qwen Coder 的推荐提示词

```text
芯片：MSPM0G3519
SDK：MSPM0 SDK 2.11.00.07
CPUCLK_FREQ：32 MHz

现有外设初始化由 SYSCFG_DL_init() 完成。
不要直接修改 ti_msp_dl_config.c 和 ti_msp_dl_config.h。
优先使用工程现有的 ADCMulti_*、ADC1Fast_*、DACOutput_*、
PWMOutput_*、SPWM_*、Key_*、OLED_* 和 LCD8080_* 封装。

禁止猜测 DL_ 函数名、枚举和参数；调用前必须依据我提供的
DriverLib 头文件、这份指南或 LP_MSPM0G3519 官方例程。
如果信息不足，请明确指出需要哪个 dl_*.h 或哪个官方例程。

修改后请列出：
1. 修改了哪些文件；
2. 是否需要调整 .syscfg；
3. 需要我在 CCS 中验证哪些编译或硬件现象。
```

## 14. 每次应该给模型哪些文件

普通业务逻辑修改：

```text
本指南
empty.c（主程序）
相关模块的 .c/.h
Debug/ti_msp_dl_config.h
编译错误
```

底层外设修改时再补：

```text
Debug/ti_msp_dl_config.c
empty.syscfg
对应的 driverlib/dl_*.h
一个 LP_MSPM0G3519 官方例程
```

不需要把整个 SDK 一次性塞入上下文。完整 SDK、文档和例程作为离线资料库，需要时检索即可。

