# Power Metrics 原理与使用方式

## 原理

Power Metrics 对一块同步电压、电流数据同时计算直流、交流和功率指标。输入采用通用整数单位；推荐电压使用 mV、电流使用 mA，这样所有功率自然以 uW 表示。

### 平均值、总RMS和交流RMS

平均电压、平均电流表示直流分量。总RMS同时包含直流和交流，代表对电阻发热能力等价的有效值。交流RMS先从每个采样中减去平均值，再计算剩余波动的RMS。

例如一个5 V直流输出叠加100 mV RMS纹波：

```text
平均电压约为5000 mV
交流RMS约为100 mV
总RMS略大于5000 mV
```

不能把总RMS直接称为纹波RMS。

### 有功、视在功率与功率因数

有功功率是瞬时功率的平均值，表示平均能量传输速度。视在功率是电压总RMS与电流总RMS的乘积。功率因数是有功功率与视在功率之比，既受相位差影响，也受非正弦波形畸变影响，因此这里得到的是真功率因数。

`powerFactorQ15` 中32768代表+1，-32768代表-1。负功率因数表示平均功率方向与约定正方向相反。

`reactivePowerMagnitude` 根据视在功率和有功功率估算。在纯正弦稳态下它对应无功功率绝对值；在非正弦或含谐波情况下，更准确的名称是“非有功功率幅值”。仅凭 P 和 S 无法判断电流超前还是滞后，所以模块不返回无功功率正负号。要判断容性或感性，需要结合相位测量。

### 直流功率与交流有功功率

`dcPower` 是平均电压乘平均电流，表示直流分量的功率。`acActivePower` 是总有功功率减去直流功率，用于区分直流偏置传输和交流信号传输。

### 峰值因数

峰值因数等于绝对峰值除以总RMS。正弦波约为1.414，直流约为1。开关电源脉冲电流的峰值因数可能很高，对器件峰值电流、磁性元件和ADC量程选择很重要。

## 基本使用方式

```c
#include "Algorithms/power_analysis/power_metrics/power_metrics.h"

#define FRAME_COUNT  (512U)

static int32_t gVoltageMilliVolt[FRAME_COUNT];
static int32_t gCurrentMilliAmpere[FRAME_COUNT];
static PowerMetrics_Result gInputMetrics;

static bool MeasureInputPower(void)
{
    if (!PowerMetrics_calculate(
            gVoltageMilliVolt,
            gCurrentMilliAmpere,
            FRAME_COUNT,
            &gInputMetrics)) {
        return false;
    }

    return gInputMetrics.valid &&
           !gInputMetrics.calculationOverflow;
}
```

若输入为 mV 和 mA，则：

```text
meanVoltage、voltageRms、voltageAcRms：mV
meanCurrent、currentRms、currentAcRms：mA
activePower、apparentPower等：uW
```

显示功率因数时，可以把 `powerFactorQ15` 除以32768；嵌入式界面也可以直接用整数拆分，避免浮点。

## 输入、输出功率与效率

四通道 `vin、iin、vout、iout` 应分别计算输入端和输出端指标：

```c
static PowerMetrics_Result gInputMetrics;
static PowerMetrics_Result gOutputMetrics;
static uint32_t gEfficiencyQ15;
static uint32_t gEfficiencyMilliPercent;

static bool MeasureEfficiency(void)
{
    if (!PowerMetrics_calculate(
            gVinMilliVolt, gIinMilliAmpere,
            FRAME_COUNT, &gInputMetrics) ||
        !PowerMetrics_calculate(
            gVoutMilliVolt, gIoutMilliAmpere,
            FRAME_COUNT, &gOutputMetrics)) {
        return false;
    }

    return PowerMetrics_calculateEfficiency(
        gInputMetrics.activePower,
        gOutputMetrics.activePower,
        &gEfficiencyQ15,
        &gEfficiencyMilliPercent);
}
```

`gEfficiencyMilliPercent=92500` 表示92.500%。若效率明显超过100%，不要直接钳位，应检查：

- 输入、输出电压和电流的校准；
- 电流方向；
- 通道时间偏差；
- 测量区间是否相同；
- 输入或输出是否还存在未测量的功率通道；
- ADC量程是否削顶。

模块允许输出超过100%，用于暴露测量问题。

## 不同题型中的使用

电源题：

```text
输入有功功率、输出有功功率、效率
输出直流电压、负载电流
输入功率因数、脉冲电流峰值因数
```

功放题：

```text
电源侧直流输入功率
负载侧交流有功功率
功放效率
输出电压/电流RMS
```

交流电子仪表题：

```text
True RMS
真功率因数
有功功率、视在功率
非有功功率幅值
```

## 注意事项

- 电压、电流必须同步。相位偏差对低功率因数负载尤其敏感。
- 测量窗口应覆盖整数个周期或足够多周期，否则各项指标会随块边界变化。
- 输入单位可以自定，但电压和电流整个处理链必须保持一致并记录。
- RMS、有功功率和功率因数必须使用校准后的原始波形，不能使用为 FFT 加窗后的数据。
- 不要在功率计算前分别把电压、电流归一化到满量程，否则真实功率和功率因数会失去意义。
- `reactivePowerMagnitude` 在畸变波形下不是传统基波无功功率。
- 结果使用64位功率累加；若 `calculationOverflow=true`，本帧所有派生指标均不可用。
- 功率因数为零可能表示纯无功、没有信号，或有功功率正负抵消，应结合RMS判断。
- 效率使用有功功率，不能用视在功率、峰值功率或平均电压乘平均电流代替。

