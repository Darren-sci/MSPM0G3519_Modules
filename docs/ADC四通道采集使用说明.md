# ADC0 四通道采集使用说明

## 1. 功能概述

本模块使用 MSPM0G3519 的 ADC0 完成四通道序列采样。TIMG0 周期事件启动一次采样帧，ADC0 按固定顺序转换四个输入，FIFO 将结果打包后由 DMA_CH0 搬入 Ping/Pong 双缓冲区。

数据通路如下：

```text
TIMG0 周期事件
      ↓
ADC0：MEM0 → MEM1 → MEM2 → MEM3
      ↓
ADC FIFO（两个 16 位结果组成一个 32 位字）
      ↓
DMA_CH0
      ↓
Ping / Pong 缓冲区
```

驱动文件：

- `Drivers/adc_multi.h`
- `Drivers/adc_multi.c`

测试显示文件：

- `Graphics/adc_multi_visualization.h`
- `Graphics/adc_multi_visualization.c`

## 2. 引脚与通道顺序

| 序列位置 | ADC通道 | MCU引脚 | 默认用途 | 数据成员 |
|---|---:|---|---|---|
| MEM0 | ADC0.0 | PA27 | 输入电压 | `vin` |
| MEM1 | ADC0.1 | PA26 | 输入电流 | `iin` |
| MEM2 | ADC0.2 | PA25 | 输出电压 | `vout` |
| MEM3 | ADC0.3 | PA24 | 输出电流 | `iout` |

四个引脚当前没有与 LCD、按键、PWM 或 SWD 调试接口冲突。

> ADC 引脚只能输入 VSSA～VDDA 范围内的电压。测量高压、负压或双极性信号前，必须使用分压、偏置、运放缓冲和输入保护电路，禁止把赛题信号直接接到 MCU。

## 3. 采样率含义

驱动中的 `frameRateHz` 表示每个通道每秒得到的样本数。一帧包含四次 ADC 转换：

```text
100 kframe/s = 每通道 100 kSPS = ADC0 总转换率约 400 kSPS
```

当前允许范围为 1 kframe/s～100 kframe/s，默认启动值为 10 kframe/s。默认值较低是为了让演示程序有足够时间刷新 LCD；只做算法处理、不频繁刷新屏幕时可以提高采样率。

TIMG0 使用 32 MHz BUSCLK。驱动根据目标采样率计算整数装载值，并通过 `ADCMulti_getActualFrameRate()` 返回实际帧率。频率、RMS、FFT 等算法应使用实际帧率。

## 4. DMA双缓冲

每块缓冲区包含 512 帧：

```text
512帧 × 4通道 × 2字节 = 4096字节
```

两块缓冲区共占用 8192 字节 RAM。DMA 写入一块时，CPU 可以处理另一块。缓冲区在内存中的逻辑排列为：

```text
vin0, iin0, vout0, iout0,
vin1, iin1, vout1, iout1,
...
```

如果 CPU 长时间不释放缓冲区，驱动会保护 CPU 正在读取的数据，丢弃新的数据块并增加 `overrunCount`。显示页面中 `OVERRUN` 应尽量保持为 0。

## 5. 基本用法

```c
#include "Drivers/adc_multi.h"

const ADCMulti_Frame *frames;
uint16_t frameCount;

SYSCFG_DL_init();
ADCMulti_init();

if (!ADCMulti_start(10000U)) {
    /* 采样率非法或定时器无法表示。 */
}

while (1) {
    if (ADCMulti_getReadyBuffer(&frames, &frameCount)) {
        /* 在这里读取 frames[i].vin、iin、vout、iout。 */

        ADCMulti_releaseBuffer(frames);
    }
}
```

停止采集：

```c
ADCMulti_stop();
```

重新设置采样率时，应停止后重新启动：

```c
ADCMulti_stop();
ADCMulti_start(50000U);
```

`ADCMulti_start()` 内部也会先安全停止旧采集，因此直接用新采样率再次调用同样有效。

## 6. 接线验证

首次验证建议给四个引脚分别输入四个不同且稳定的安全电压，例如 0.5 V、1.0 V、1.5 V、2.0 V，并检查 LCD 上四行数值是否一一对应。

建议依次检查：

1. 四个通道顺序是否正确；
2. 输入接地时零点是否稳定；
3. 输入接近 VDDA 时是否饱和；
4. 改变某一路电压时其他通道是否明显跳动；
5. `RATE` 是否等于目标附近的实际帧率；
6. `OVERRUN` 是否持续增加。

当前换算使用标称 3300 mV 参考电压，只用于功能验证。正式测量应实测 VDDA，并为每个通道保存零偏、增益、分压比和采样电阻校准参数。

## 7. 与 ADC1 的关系

本模块只占用 ADC0 和 DMA_CH0。ADC1 与 DMA_CH1 保留给后续单通道高速自由运行采集，两者可以分别工作。若以后需要电压、电流严格同时采样，也可以增加双 ADC 同事件触发模式。

## 8. 使用注意事项

- 四通道是依次转换，不是四路同时采样；高频相位测量需要考虑通道时间差。
- 当前采样保持时间为 250 ns。高阻分压网络前应增加运放缓冲，或者延长采样时间。
- LCD 并口刷新会占用较多 CPU 时间，高速采集模式下不应每块数据都整屏刷新。
- DMA中断只负责切换缓冲区，不应在中断中计算 RMS、FFT 或绘图。
- 修改 SysConfig 后应重新生成 `ti_msp_dl_config.c/.h`，不要手工编辑生成文件。
