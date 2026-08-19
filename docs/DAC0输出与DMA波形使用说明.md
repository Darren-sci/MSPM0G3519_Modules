# DAC0输出与DMA波形使用说明

## 1. 硬件分配

MSPM0G3519片内DAC0的外部输出脚固定为PA15。原LCD背光已经迁移到PB16，
当前资源分配如下：

| 功能 | 资源 |
|---|---|
| DAC输出 | DAC0_OUT / PA15 |
| 波形DMA | DMA_CH2 |
| 波形更新 | DAC内部采样发生器 |
| LCD背光 | PB16普通GPIO |

DAC参考电压使用VDDA/VSSA，输出为12位无符号码0～4095。DAC输出不能直接
驱动低阻负载或功率负载，需要时应增加运放缓冲、滤波和保护。

## 2. 初始化

`SYSCFG_DL_init()`完成DAC、FIFO和DMA基础配置。随后必须调用
`DACOutput_init()`，它会关闭采样发生器和DMA，并把输出设为0码：

```c
#include "ti_msp_dl_config.h"
#include "Drivers/dac_output.h"

int main(void)
{
    SYSCFG_DL_init();
    DACOutput_init();

    while (1) {
        __WFI();
    }
}
```

当前主程序已经调用 `DACOutput_init()`，但不会自动启动波形输出。

## 3. 输出直流电压

直接设置12位DAC码：

```c
DACOutput_setCode(2048U);
```

根据参考电压换算：

```c
/* 假设实测VDDA为3300mV，目标输出1250mV。 */
DACOutput_setMilliVolts(1250U, 3300U);
```

VDDA不是精密基准，要求较高时应实测参考电压并标定DAC增益和零点。
`setCode()` 和 `setMilliVolts()` 会先停止正在运行的DMA波形。

## 4. DMA循环波形

波形表必须使用 `static` 或全局存储，在停止波形之前不能释放或修改：

```c
static const uint16_t gTriangle8[8] = {
    0U, 1024U, 2048U, 3072U,
    4095U, 3072U, 2048U, 1024U
};

void Waveform_start(void)
{
    DACOutput_startWaveform(
        gTriangle8,
        8U,
        DAC_OUTPUT_RATE_100_KHZ);
}
```

上述波形的理论重复频率为：

```text
100000 ÷ 8 = 12500 Hz
```

也可以通过接口读取，单位为mHz：

```c
uint64_t frequencyMilliHz =
    DACOutput_getWaveformFrequencyMilliHz();
```

停止波形：

```c
DACOutput_stopWaveform();
```

停止后DAC保持最近电平。如需安全回零，应继续调用：

```c
DACOutput_setCode(0U);
```

## 5. 支持的更新率

内部采样发生器支持以下离散更新率：

```text
500、1k、2k、4k、8k、16k、100k、200k、500k、1M samples/s
```

波形重复频率等于“更新率÷表长度”。例如1 MS/s配合64点表，只能得到
15.625 kHz波形。若需要任意更新率，可以后续改用通用定时器事件触发DAC。

## 6. FIFO欠载

DMA或总线未及时补充数据时会产生FIFO欠载，输出波形可能出现停顿。可监控：

```c
uint32_t underruns = DACOutput_getUnderrunCount();
```

正常运行时该值应保持为0。若持续增长，应降低DAC更新率、减少同时运行的
高速DMA业务，或检查波形表和DMA配置。

## 7. 与ADC同时运行

当前DMA分配为：

| DMA通道 | 用途 |
|---|---|
| DMA_CH0 | ADC0四通道 |
| DMA_CH1 | ADC1高速单通道 |
| DMA_CH2 | DAC0循环波形 |

三者可以同时配置，但ADC1最高速采集和DAC 1 MS/s输出同时运行时会增加DMA
总线压力。只需要直流DAC时，应使用 `DACOutput_setCode()`，不要启动波形DMA。

## 8. 适用范围

片内DAC适合：

- PI/PID控制电压；
- 偏置和比较参考；
- 低频测试波形；
- 外部增益或功率设定。

片内DAC最高更新率为1 MS/s，不能生成2024 A题要求的2 MHz高质量正弦波。
MHz级信号源仍应使用外部DDS，片内DAC可负责慢速幅度或偏置控制。
