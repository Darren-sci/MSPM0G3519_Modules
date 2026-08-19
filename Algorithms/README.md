# Algorithms 赛题选用导航

本目录的每个算法子目录都包含 `.h`、`.c` 和中文使用说明。一般先根据题目
选择采集通道，再选择下表中的算法；不要为了“功能齐全”而在同一次采集中
运行全部算法。

## 1. 当前模拟引脚与资源

| 用途 | 引脚/资源 | 适用情况 |
|---|---|---|
| ADC0 VIN | PA27 / ADC0_CH0 | 输入电压或第1路同步信号 |
| ADC0 IIN | PA26 / ADC0_CH1 | 输入电流或第2路同步信号 |
| ADC0 VOUT | PA25 / ADC0_CH2 | 输出电压或BTL输出正端 |
| ADC0 IOUT | PA24 / ADC0_CH3 | 输出电流或BTL输出负端 |
| ADC1高速单通道 | PB27 / ADC1_CH14 | 波形、频率、FFT、单次脉冲 |
| DAC0输出 | PA15 / DAC0_OUT | 控制电压、偏置和低频测试波形 |
| PWM/SPWM | PA0、PA1 | 功率级控制或滤波后低频正弦 |
| LCD背光 | PB16 GPIO | 仅控制背光，不再占用PA15 |

ADC0四路为同步的一帧顺序采样，每通道最高配置为100 kS/s；ADC1是不使用
定时器的高速连续采集。引脚只能接经过限压、偏置和滤波的0～VDDA信号。

## 2. 按题型选择

| 题型/需求 | 推荐资源 | 推荐算法 |
|---|---|---|
| A类：运放增益、带宽、压摆率、静态功耗 | PB27高速看波形；PA27/PA25同步比较输入输出；外部DDS产生MHz信号 | `SignalAnalyzer`、`frequency_response`、`peak_to_peak`、`rms`、`power_metrics` |
| B类：音频功放、BTL功率、自动音量 | PA27采输入，PA25/PA24采BTL两端；PA15或PA0控制增益 | `rms`、`power_metrics`、`overrange_detection`、`pi` |
| C类：示波、频率、占空比、FFT、谐波、单次脉冲 | PB27高速采集；LCD数据与控制引脚见LCD文档 | `SignalAnalyzer`、`threshold_trigger`、`ring_buffer` |
| D类：电源/光伏模拟、稳压稳流、效率与纹波 | PA27/PA26采输入，PA25/PA24采输出；PA0/PA1驱动功率级 | `pi`或`pid`、`power_metrics`、`ripple`、`overrange_detection` |
| 滤波与预处理 | 不增加引脚 | `adc_calibration` → `median/moving_average/FIR/IIR` → 后续测量 |
| 阻抗、相位、扫频 | 两路ADC同步采电压和电流 | `pll`、`synchronous_detection`、`impedance_measurement`、`frequency_response` |

> A题要求的2 MHz正弦不能由片内DAC产生。PA15片内DAC最高更新率为
> 1 MS/s，只适合慢速激励和控制量；MHz信号源应另接外部DDS，其SPI引脚
> 需要在确定模块型号后再分配。

## 3. A类：高速单通道参数分析示例

下面用PB27采集1024点，计算基础参数、频率、频谱和THD。分析器与工作区
较大，必须定义为全局或 `static`。

```c
#include "Drivers/adc1_fast.h"
#include "Algorithms/analysis_pipeline/signal_analyzer.h"

static SignalAnalyzer gAnalyzer;
static SignalAnalyzer_Workspace gWorkspace;
static SignalAnalyzer_Result gResult;

void ATask_init(uint32_t measuredSampleRateHz)
{
    SignalAnalyzer_Config cfg;
    SignalAnalyzer_getDefaultConfig(
        &cfg, measuredSampleRateHz, ADC1_FAST_SAMPLE_COUNT);
    cfg.rawCalibrationEnabled = ADCCalibration_initBipolarQ15(
        &cfg.rawCalibration, 0U, 2048U, 4095U);
    cfg.enabledFeatures |= SIGNAL_ANALYZER_FEATURE_SPECTRUM |
        SIGNAL_ANALYZER_FEATURE_FUNDAMENTAL |
        SIGNAL_ANALYZER_FEATURE_HARMONICS |
        SIGNAL_ANALYZER_FEATURE_THD;
    cfg.windowType = SIGNAL_ANALYZER_WINDOW_HANN;
    (void)SignalAnalyzer_init(&gAnalyzer, &cfg, &gWorkspace);
    ADC1Fast_init();
    (void)ADC1Fast_start();
}

void ATask_poll(void)
{
    const uint16_t *samples;
    uint16_t count;
    uint32_t consumed;

    if (ADC1Fast_getReadyBuffer(&samples, &count)) {
        (void)SignalAnalyzer_pushRawADC(&gAnalyzer, samples, count, 1U,
            &consumed, &gResult);
        ADC1Fast_releaseBuffer(samples);
        /* RESULT_READY后读取gResult.frequency、peakToPeak、thd等。 */
    }
}
```

`measuredSampleRateHz` 应使用示波器或计时基准实测后填写；ADC1无定时器，
不能把一个估计值当作精密采样率。

## 4. B类：BTL输出功率示例

PA25、PA24分别采BTL两端，先用各自标定参数换算成mV。4 Ω纯电阻负载下，
差分电压的RMS平方除以4000 mΩ即可得到mW。

```c
#include "Drivers/adc_multi.h"
#include "Algorithms/preprocessing/adc_calibration/adc_calibration.h"
#include "Algorithms/measurements/rms/rms.h"

static ADCCalibration_Config gOutPlusCal;
static ADCCalibration_Config gOutMinusCal;
static int16_t gDifferentialMv[ADC_MULTI_FRAME_COUNT];

void BTask_init(void)
{
    /* 示例量程为-3300～+3300 mV；必须替换成模拟前端的实测标定值。 */
    (void)ADCCalibration_init(
        &gOutPlusCal, 0U, 2048U, 4095U, -3300, 3300);
    (void)ADCCalibration_init(
        &gOutMinusCal, 0U, 2048U, 4095U, -3300, 3300);
}

void BTask_process(const ADCMulti_Frame *frames, uint16_t count)
{
    uint32_t i;
    uint32_t vrmsMv;
    uint64_t powerMw;

    for (i = 0U; i < count; i++) {
        int32_t plusMv = ADCCalibration_apply(&gOutPlusCal, frames[i].vout);
        int32_t minusMv = ADCCalibration_apply(&gOutMinusCal, frames[i].iout);
        gDifferentialMv[i] = (int16_t)(plusMv - minusMv);
    }
    if (RMS_calculateAC(gDifferentialMv, count, &vrmsMv, 0)) {
        powerMw = ((uint64_t)vrmsMv * vrmsMv) / 4000U;
        /* 显示powerMw，或把它作为PI自动音量控制的测量值。 */
    }
}
```

实际使用前必须分别调用 `ADCCalibration_init()` 设置两路毫伏标定，并保证
前端允许测量BTL两端的共模电压。

## 5. C类：示波与触发示例

连续参数、FFT和THD直接复用A类的 `SignalAnalyzer`。单次脉冲或稳定波形
显示时，再给校准后的数据增加阈值触发：

```c
#include "Algorithms/detection/threshold_trigger/threshold_trigger.h"

static ThresholdTrigger_State gTrigger;

void ScopeTrigger_init(void)
{
    const ThresholdTrigger_Config cfg = {
        .threshold = 0, .hysteresis = 300U,
        .holdoffSamples = 100U, .edge = THRESHOLD_TRIGGER_RISING
    };
    (void)ThresholdTrigger_init(&gTrigger, &cfg);
}

bool ScopeTrigger_push(int16_t sample, ThresholdTrigger_Event *event)
{
    bool triggered = false;
    (void)ThresholdTrigger_processSample(
        &gTrigger, sample, &triggered, event);
    return triggered;
}
```

需要显示触发前波形时，把连续样本放入 `ring_buffer`；检测到触发后再保存
规定数量的后触发点。LCD完整接线见 `../docs/LCD屏幕模块使用说明.md`。

## 6. D类：PI稳压控制示例

下例把PA25采到的输出电压换算为mV，再由PI控制PA0 PWM。控制函数必须由
固定1 ms周期定时调用，不能放在不定时的主循环里。

```c
#include "Algorithms/control/pi/pi.h"
#include "Algorithms/preprocessing/adc_calibration/adc_calibration.h"
#include "Drivers/pwm_output.h"

static PI_Controller gVoltageLoop;
static ADCCalibration_Config gVoutCal;

void DTask_init(void)
{
    const PI_Config cfg = {
        .kpQ16 = 6554, .kiQ16PerSecond = 32768,
        .samplePeriodUs = 1000U,
        .outputMin = 0, .outputMax = 1000,
        .integratorMin = 0, .integratorMax = 1000,
        .outputRiseLimit = 20U, .outputFallLimit = 50U,
        .action = PI_ACTION_DIRECT,
        .antiWindup = PI_ANTI_WINDUP_CONDITIONAL
    };
    /* 示例把0～4095码映射为0～50000 mV，按实际分压比重新标定。 */
    (void)ADCCalibration_init(
        &gVoutCal, 0U, 0U, 4095U, 0, 50000);
    (void)PI_init(&gVoltageLoop, &cfg);
    (void)PI_reset(&gVoltageLoop, 0);
}

void DTask_controlTick(uint16_t rawVout, int32_t targetMv)
{
    PI_Result result;
    int32_t measuredMv = ADCCalibration_apply(&gVoutCal, rawVout);

    if (PI_update(&gVoltageLoop, targetMv, measuredMv, &result)) {
        PWMOutput_setChannel0Duty((uint16_t)result.output);
    }
}
```

功率级调试前应先配置过流硬件保护；软件可再用 `overrange_detection` 做慢速
故障确认。输入/输出效率用 `power_metrics`，输出纹波用 `ripple`。

## 7. 推荐阅读顺序

1. 先读对应题型的驱动文档和本页示例。
2. 普通信号测量优先使用 `analysis_pipeline/SignalAnalyzer统一分析接口使用说明.md`。
3. 只有统一接口未覆盖的专用需求，再进入相应算法目录查看详细文档。
4. 上板前确认采样率、ADC标定、信号单位和缓冲区生命周期。
