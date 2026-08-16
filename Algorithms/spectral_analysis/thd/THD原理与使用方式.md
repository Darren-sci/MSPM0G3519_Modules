# THD 原理与使用方式

## 原理

THD 表示总谐波失真。它先把二次及以上各有效谐波的幅值平方相加，再开平方得到所有谐波的合成有效幅值，最后除以基波幅值。

不能直接把“二次谐波幅值加三次谐波幅值”作为 THD，因为不同频率的正弦分量应按平方和合成。当前实现使用64位整数累加功率，并用整数平方根和整数除法得到结果，不依赖浮点数学库。

本模块计算的是 THD，不是 THD+N。搜索频点以外的宽带噪声没有进入谐波数组，因此不会被计入结果。

## 输出单位

`THD_Result` 同时提供两种比例：

```text
thdQ15          32768表示1.0，即100%
thdMilliPercent 1000表示1.000%
```

例如：

```text
thdMilliPercent = 1234
```

表示 THD 为1.234%。这个整数格式便于直接在屏幕上拆分整数和三位小数。

## 使用方式

```c
#include "Algorithms/spectral_analysis/thd/thd.h"

static THD_Result gTHD;

static bool CalculateTHD(void)
{
    if (!THD_calculate(
            gHarmonics,
            HARMONIC_COUNT,
            &gTHD)) {
        return false;
    }

    if (gTHD.powerSaturated) {
        /* 谐波功率累加已经超过64位范围，结果不可继续用于测量。 */
        return false;
    }

    return gTHD.valid;
}
```

显示三位小数百分比：

```c
uint32_t integerPart = gTHD.thdMilliPercent / 1000U;
uint32_t fractionalPart = gTHD.thdMilliPercent % 1000U;

/* 显示为 integerPart.fractionalPart %。 */
```

完整处理顺序为：

```text
ADC校准与去直流
→ Hann或Flat-top加窗
→ FFT
→ Spectrum配置
→ Fundamental Detection
→ Harmonic Analysis
→ THD
```

## 注意事项

- `components[0]` 必须是有效的1次基波，且基波幅值不能为零。
- 只有 `valid=true` 的2次及以上分量进入THD计算。
- `includedHarmonicCount` 表示实际参与平方和的谐波数量。
- THD准确性首先取决于ADC动态范围、窗函数、采样长度、基波检测和谐波提取准确性。
- 强基波的频谱泄漏可能覆盖很弱的谐波，造成THD偏高或偏低。
- 输入前端、信号源和ADC自身的失真都会进入测量结果。
- 若需要 THD+N、SINAD、SNR 或 ENOB，应建立独立模块并明确噪声带宽，不能把本THD结果直接当作THD+N。
- `thdMilliPercent` 可能超过100%，这不一定是计算溢出，但通常意味着基波选择错误、输入不是以基波为主，或被测波形严重失真。
