# ADC1高速单通道DMA使用说明

## 1. 功能定位

ADC1用于独立的高速单通道波形采集，适合FFT、波形识别、单次触发和脉冲
分析。它与ADC0四通道测量互相独立：

| 功能 | ADC0 | ADC1 |
|---|---|---|
| 用途 | VIN、IIN、VOUT、IOUT四通道测量 | 单通道高速波形 |
| 触发方式 | TIMG0定时事件 | 软件启动后连续转换 |
| DMA | DMA_CH0 | DMA_CH1 |
| 引脚 | PA27～PA24 | PB27 |
| ADC通道 | 0～3 | 14 |

ADC1不占用额外定时器。调用一次 `ADC1Fast_start()` 后，ADC自身连续重复
转换，DMA自动把结果搬入双缓冲区。

## 2. 当前配置

- ADC实例：ADC1
- 输入引脚：PB27
- 输入通道：ADC1通道14
- 参考电压：VDDA/VSSA
- 数据格式：12位无符号
- ADC时钟：SYSOSC 32 MHz，1分频
- 采样保持时间：62.5 ns
- 转换模式：重复单通道、自动连续采样
- 触发源：软件触发
- FIFO：开启，两个16位结果打包成一个32位字
- DMA通道：DMA_CH1
- 单块长度：1024点
- 缓冲方式：两块交替使用，共占4096字节SRAM

采样率由ADC时钟、采样保持时间和12位转换周期共同决定，不由定时器设定。
如果需要一个精确可调的采样率，应另建定时触发模式，而不是使用本高速模式。

## 3. 最小启动代码

```c
#include "ti_msp_dl_config.h"
#include "Drivers/adc1_fast.h"

int main(void)
{
    const uint16_t *samples;
    uint16_t sampleCount;

    SYSCFG_DL_init();
    ADC1Fast_init();

    if (!ADC1Fast_start()) {
        while (1) {
            /* 启动失败处理。 */
        }
    }

    while (1) {
        if (ADC1Fast_getReadyBuffer(&samples, &sampleCount)) {
            /*
             * samples包含1024个12位无符号ADC码。
             * 在此进行校准、FFT、触发或波形显示。
             */

            ADC1Fast_releaseBuffer(samples);
        }
    }
}
```

必须先调用 `SYSCFG_DL_init()`，再调用 `ADC1Fast_init()`。结束使用时调用：

```c
ADC1Fast_stop();
```

## 4. 与SignalAnalyzer配合

ADC1返回原始12位无符号码。交流信号通常由模拟前端偏置到ADC量程中点，
因此应先配置双极性Q15校准：

```c
#include "Algorithms/analysis_pipeline/signal_analyzer.h"

static SignalAnalyzer gAnalyzer;
static SignalAnalyzer_Workspace gWorkspace;
static SignalAnalyzer_Result gResult;

static void Analysis_init(uint32_t measuredSampleRateHz)
{
    SignalAnalyzer_Config config;

    SignalAnalyzer_getDefaultConfig(
        &config, measuredSampleRateHz, ADC1_FAST_SAMPLE_COUNT);

    config.rawCalibrationEnabled =
        ADCCalibration_initBipolarQ15(
            &config.rawCalibration,
            0U,       /* 负满量程ADC码，应换成实测值 */
            2048U,    /* 零输入偏置码，应换成实测值 */
            4095U);   /* 正满量程ADC码，应换成实测值 */

    config.enabledFeatures |=
        SIGNAL_ANALYZER_FEATURE_SPECTRUM |
        SIGNAL_ANALYZER_FEATURE_FUNDAMENTAL |
        SIGNAL_ANALYZER_FEATURE_HARMONICS |
        SIGNAL_ANALYZER_FEATURE_THD;

    (void) SignalAnalyzer_init(&gAnalyzer, &config, &gWorkspace);
}

static void Analysis_process(const uint16_t *samples)
{
    uint32_t consumed;

    if (SignalAnalyzer_pushRawADC(
            &gAnalyzer,
            samples,
            ADC1_FAST_SAMPLE_COUNT,
            1U,
            &consumed,
            &gResult) == SIGNAL_ANALYZER_STATUS_RESULT_READY) {
        /* 根据gResult.validFeatures读取有效结果。 */
    }
}
```

高速模式的实际采样率不是由软件定时器直接给出的。正式计算频率前，应使用
已知频率信号校准采样率，或根据器件时序确认准确转换周期。

## 5. 双缓冲使用规则

DMA写一块时，CPU可以处理另一块。使用规则是：

1. `ADC1Fast_getReadyBuffer()`取得只读指针。
2. 完成计算或复制后调用 `ADC1Fast_releaseBuffer()`。
3. 不得修改返回缓冲区，也不得在释放后继续保存该指针。
4. 每次成功取得的缓冲区只能释放一次。

若CPU处理速度赶不上ADC，驱动会优先保护CPU正在读取的缓冲区，并丢弃旧
数据。可通过以下接口监控：

```c
uint32_t droppedBlocks = ADC1Fast_getOverrunCount();
```

高速采集时不要对每个数据块进行全屏LCD刷新。建议先完成必要计算，再把
波形抽取为几百个显示点，并将LCD刷新限制在约10～20帧每秒。

## 6. 模拟输入注意事项

- PB27输入必须位于VSSA～VDDA范围内，不能直接输入负电压或超过VDDA。
- 62.5 ns采样保持时间要求模拟前端具有较低输出阻抗和足够驱动能力。
- 高阻信号应增加运放缓冲，或在SysConfig中适当延长采样保持时间。
- 输入端应根据目标带宽设计抗混叠低通滤波器。
- 交流双极性信号必须先经过衰减、保护和中点偏置。
- 首次启动和切换输入量程后，可丢弃第一块数据，让模拟前端稳定。

## 7. 与其他模块同时使用

ADC1高速采集可以与ADC0四通道同时配置，因为两者使用不同ADC和DMA通道。
但同时运行会增加SRAM、DMA总线和CPU处理压力：

- ADC0使用DMA_CH0，ADC1使用DMA_CH1，不存在通道号冲突。
- ADC1不占用TIMG0，ADC0仍可保持定时触发。
- 如果只做高速波形分析，可不调用 `ADCMulti_init()` 和
  `ADCMulti_start()`。
- 如果只做四通道功率测量，可不调用 `ADC1Fast_init()` 和
  `ADC1Fast_start()`。

SysConfig中保留ADC1只会占用PB27、ADC1和DMA_CH1资源；只有调用
`ADC1Fast_start()` 后才会持续进行高速采样。

## 8. 连续性的边界

当前DMA在每个1024点块完成后由ADC1中断立即装载下一块。采样转换本身由
ADC连续运行，但DMA重新装载存在很短的中断延迟，因此该模式适合分块FFT、
波形识别和触发检测，不应直接宣称为绝对无间隙记录。

如果以后需要长时间严格无缝采样，应进一步采用硬件DMA重复/乒乓机制，或
在实测确认FIFO能够覆盖DMA重装延迟后再作为无缝采集使用。
