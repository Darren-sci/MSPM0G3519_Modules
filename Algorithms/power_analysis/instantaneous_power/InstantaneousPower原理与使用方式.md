# Instantaneous Power 原理与使用方式

## 原理

瞬时功率由同一时刻的电压与电流相乘得到。电压、电流同号时功率为正，表示能量沿定义的正方向传递；异号时功率为负，表示负载向电源回送能量或电流方向定义相反。

对于交流信号，瞬时功率会随时间变化，单独一个采样没有稳定测量意义。把完整周期内的瞬时功率取平均，才得到有功功率。不能简单地用“平均电压乘平均电流”替代，因为纯交流信号的电压、电流平均值可能都为零，但负载仍然消耗功率。

模块采用通用整数单位。推荐先把 ADC 校准为 mV 和 mA：

```text
mV × mA = uW
```

例如5000 mV乘2000 mA得到10000000 uW，也就是10 W。若电压、电流均使用 Q15，则乘积是 Q30，后续必须按 Q30 解释。

## 关键函数

- `InstantaneousPower_calculateSample()`：计算一对同步采样的瞬时功率。
- `InstantaneousPower_calculateBlock()`：输出整块瞬时功率波形。
- `InstantaneousPower_accumulateSample()`：把一对采样加入跨数据块累计器。
- `InstantaneousPower_accumulateBlock()`：累计整个 ADC 数据块，不需要保存瞬时功率数组。
- `InstantaneousPower_getAverage()`：返回累计区间的平均有功功率。
- `InstantaneousPower_getEnergy()`：用采样率对累计功率积分，得到能量。

累计器使用有符号64位总和，并检查总和及计数饱和。饱和后必须调用 `InstantaneousPower_resetAccumulator()`，不能继续相信旧结果。

## 直接计算功率波形

```c
#include "Algorithms/power_analysis/instantaneous_power/instantaneous_power.h"

#define FRAME_COUNT  (512U)

static int32_t gVoltageMilliVolt[FRAME_COUNT];
static int32_t gCurrentMilliAmpere[FRAME_COUNT];
static int64_t gPowerMicroWatt[FRAME_COUNT];

static bool BuildPowerWaveform(void)
{
    return InstantaneousPower_calculateBlock(
        gVoltageMilliVolt,
        gCurrentMilliAmpere,
        gPowerMicroWatt,
        FRAME_COUNT);
}
```

瞬时功率波形适合观察开关电源的能量传输、功放输出功率包络、反向功率区间和异常尖峰。如果只需要平均功率，不必分配这块 `int64_t` 数组，直接使用累计器更省RAM。

## 跨数据块测量平均功率和能量

```c
static InstantaneousPower_Accumulator gInputEnergy;

static void PowerMeasurement_start(void)
{
    InstantaneousPower_resetAccumulator(&gInputEnergy);
}

static bool PowerMeasurement_addFrame(void)
{
    return InstantaneousPower_accumulateBlock(
        &gInputEnergy,
        gVoltageMilliVolt,
        gCurrentMilliAmpere,
        FRAME_COUNT);
}

static bool PowerMeasurement_finish(
    uint32_t sampleRateHz,
    int64_t *averageMicroWatt,
    int64_t *energyMicroJoule)
{
    return InstantaneousPower_getAverage(
               &gInputEnergy, averageMicroWatt) &&
           InstantaneousPower_getEnergy(
               &gInputEnergy, sampleRateHz, energyMicroJoule);
}
```

当功率单位为 uW 时，能量输出单位为 uJ；累计1秒、平均功率10 W时，结果为10000000 uJ，即10 J。采样率在整个累计期间必须保持不变。

## 同步与极性

电压和电流必须代表同一个采样时刻。如果两个通道存在固定时间偏差，交流功率因数和有功功率都会产生误差。多通道 ADC 若是依次转换而非真正同时采样，需要根据通道间隔、信号最高频率判断是否需要相位补偿。

电流传感器的正方向必须统一。例如规定“流入负载为正”，那么负载吸收功率通常为正。若测得稳定负功率，应先检查电流传感器方向、差分放大器极性和校准系数，不要直接对功率取绝对值。

## 注意事项

- 输入必须先完成零点、增益和正负极性校准。
- 电压和电流数组必须长度相同、采样率相同并且时间对齐。
- `calculateBlock()` 的输入和输出不能重叠；一个功率点占8字节，注意RAM占用。
- 平均区间最好包含整数个工频、基波或开关周期，避免截断产生波动。
- 平均有功功率可能为负，这是四象限测量中的有效结果，不应自动钳位为零。
- 能量计算使用调用者提供的采样率；采样率配置错误会等比例影响能量结果。
- 长时间累计可能使64位总和饱和，应定期读取、保存能量并重新开始累计。
- 瞬时功率模块不计算 RMS、视在功率、功率因数和效率，这些由 Power Metrics 完成。

