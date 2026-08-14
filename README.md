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

四通道 ADC0 的引脚、采样率和 API 说明见
[docs/ADC四通道采集使用说明.md](docs/ADC四通道采集使用说明.md)。

使用时先阅读 [LCD屏幕模块使用说明](docs/LCD屏幕模块使用说明.md)。更完整的
英文 API 说明见 [docs/lcd_usage.md](docs/lcd_usage.md)。
