# MSPM0G3519 5-inch LCD module

This project contains a standalone driver for the same 480x854, RGB565,
16-bit 8080 LCD used by the MSPM0G3507 project in `D:\TI\Project\G`.

The module includes:

- MSPM0G3519 SysConfig pin assignments
- 16-bit GPIO 8080 bus driver
- panel initialization sequence and landscape mode
- pixel, rectangle, line and circle drawing
- built-in ASCII font
- UTF-8 Chinese software-font interface without a W25Q64 dependency
- ADC0 four-channel timer-triggered sequence capture with DMA ping-pong buffers
- ADC1 maximum-speed single-channel capture with DMA ping-pong buffers
- DAC0 direct-voltage and DMA waveform output

四通道 ADC0 的引脚、采样率和 API 说明见
[docs/ADC四通道采集使用说明.md](docs/ADC四通道采集使用说明.md)。

ADC1 高速单通道说明见
[docs/ADC1高速单通道DMA使用说明.md](docs/ADC1高速单通道DMA使用说明.md)，
DAC0 直流与 DMA 波形输出说明见
[docs/DAC0输出与DMA波形使用说明.md](docs/DAC0输出与DMA波形使用说明.md)。

使用时先阅读 [LCD屏幕模块使用说明](docs/LCD屏幕模块使用说明.md)。更完整的
英文 API 说明见 [docs/lcd_usage.md](docs/lcd_usage.md)。

算法按赛题类型、引脚和需求的选用方法见
[Algorithms/README.md](Algorithms/README.md)。

## Main function feature switches

`empty.c` 顶部提供 LCD、ADC0、ADC1、SignalAnalyzer、DAC、固定 PWM 和
SPWM 的编译开关。把对应的 `ENABLE_*` 改为 `1` 即可编译该功能，改为
`0` 后相关变量和代码不会占用 SRAM。当前默认配置用于ADC1测试：LCD、
ADC1高速采集和SignalAnalyzer开启，ADC0关闭。源码还会在编译期阻止同一
PWM引脚同时选择固定 PWM 与 SPWM、
以及 DAC0 同时选择直流和 DMA 波形。

ADC1波形、频谱页面和信号发生器接法见
[docs/ADC1频谱与波形显示测试说明.md](docs/ADC1频谱与波形显示测试说明.md)。
