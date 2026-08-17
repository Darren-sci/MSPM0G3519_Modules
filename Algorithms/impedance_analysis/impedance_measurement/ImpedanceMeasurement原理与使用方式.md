# Impedance Measurement 原理与使用方式

## 原理

阻抗是同一频率下复电压与复电流之比。同步检波已经把电压、电流分别表示成 I/Q，因此模块可以直接进行复数相除，得到：

```text
R：与电流同相的电阻成分
X：与电流正交的电抗成分
|Z|：阻抗幅值
相位：电压相对于电流的相位
```

正电抗和正相位通常表示感性，负电抗和负相位通常表示容性，接近0度时归类为电阻性。分类阈值由调用者提供，避免噪声让纯电阻在“感性/容性”之间跳动。

输入电压使用 mV、电流使用 mA 时，mV/mA等于Ω，模块再乘1000后输出mΩ。

## 系统结构

```text
激励参考ADC → PLL ───────────────┐
                                  │
被测电压ADC → 同步检波 → V_I/V_Q ├→ Impedance Measurement
被测电流ADC → 同步检波 → I_I/I_Q ┘
```

电流可以通过采样电阻得到，但必须先换算成 mA。不能把采样电阻两端的 mV 直接当作 mA，除非采样电阻和增益恰好使数值等价。

## 使用方式

```c
ImpedanceMeasurement_Result impedance;

if (voltageDetection.stable && currentDetection.stable &&
    ImpedanceMeasurement_calculate(
        &voltageDetection,
        &currentDetection,
        pllOutput.frequencyMilliHz,
        phaseCorrectionMilliDegrees,
        3000U, /* ±3度以内按电阻性分类 */
        &impedance)) {
    /* magnitudeMilliOhms */
    /* resistanceMilliOhms */
    /* reactanceMilliOhms */
    /* phaseMilliDegrees */
}
```

`phaseCorrectionMilliDegrees` 用于补偿电压通道相对电流通道的固定相差。模块会同时旋转 R/X，而不是只修改显示相位。校准值可能随频率变化，高精度扫频时应使用频率相关校准表。

## L/C估算

正电抗时可以估算串联等效电感：

```c
uint64_t inductanceMicroHenry;
ImpedanceMeasurement_estimateInductanceMicroHenry(
    &impedance, &inductanceMicroHenry);
```

负电抗时可以估算串联等效电容：

```c
uint64_t capacitancePicoFarad;
ImpedanceMeasurement_estimateCapacitancePicoFarad(
    &impedance, &capacitancePicoFarad);
```

这只是单频串联等效模型。真实元件存在 ESR、寄生电感、电容和频率相关损耗，不能仅凭一个频点断言元件的完整参数。

## 校准

阻抗准确度同时依赖电压增益、电流增益和通道相位。推荐用已知精密电阻进行复数校准：理想纯电阻的相位应接近0，测得幅值与标称值之比可修正增益，测得相位可修正通道延迟。

开路时电流接近零，阻抗结果会极不稳定；短路时电压接近零，也会受偏置和噪声主导。应用层应设置最小电压、电流幅值门限。

## 注意事项

- 电压、电流必须来自同一PLL参考和同一稳定测量区间。
- 电流幅值为零时函数返回false，接近零时即使返回成功也可能没有意义。
- `stable` 应由应用层在调用前检查；本模块只检查结果有效与饱和。
- 相位校准必须考虑ADC顺序采样延迟、运放、RC滤波器、采样电阻和线缆。
- 高频下夹具和PCB寄生参数可能比被测元件本身更明显，需要开路/短路/负载校准。
- L/C估算只在电抗符号正确、频率非零且单一模型合理时使用。
- 负电阻可能表示有源电路、功率回送、极性错误或相位校准错误，不能简单取绝对值。

