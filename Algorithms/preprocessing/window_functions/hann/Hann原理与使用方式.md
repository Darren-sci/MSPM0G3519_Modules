# Hann 原理与使用方式

## 原理

FFT 会把有限长度的数据块当作一个不断重复的周期。如果数据块最后一个采样与下一个周期的第一个采样不连续，拼接位置就像突然产生了一个边沿，使单一频率的能量扩散到许多频点，这种现象称为频谱泄漏。

Hann 窗给每个时域采样乘上一个不同的权重。窗口中心的权重接近一，两端逐渐降低到零，使数据块在周期拼接处平滑衔接。泄漏因此明显减小，基波、谐波和噪声频谱更容易观察。代价是频谱主瓣变宽，距离很近的两个频率更难分开，而且正弦幅值会被窗口平均权重衰减。

当前实现是首尾严格为零的对称 Hann 窗。系数使用 Q15，最大值为 32767。初始化时通过小型四分之一正弦表和线性插值生成前半个窗口，再镜像得到后半部分，因此不需要浮点三角函数，也能保证左右严格对称。

## 关键函数

### `Hann_init()`

`Hann_init()` 根据指定长度生成全部 Q15 系数，并计算相干增益和功率增益。调用者负责提供系数缓冲区，所需元素数量必须与窗口长度相同。窗口应在系统初始化时生成一次，不要在每个 DMA 数据块到达后重复初始化。

生成时只计算前半个窗口，再把系数写到对称位置。奇数长度只有一个中心点，偶数长度则有两个对称的中心附近点。初始化要求长度至少为三，实际 FFT 通常使用 256、512 或 1024 点。

### `Hann_sineQ15()`

该内部函数利用65个四分之一正弦表值恢复完整周期。32位相位的高位选择相邻表项，其余部分确定插值比例。这样只需很小的 Flash 表格即可生成任意长度窗口。插值存在轻微定点误差，但窗口系数最终通过镜像保证对称。

### `Hann_apply()`

`Hann_apply()` 将每个 Q15 采样与对应 Q15 窗系数相乘，乘积四舍五入后恢复成 Q15。输入和输出可以是不同数组，也可以指向同一数组。原地处理会覆盖原始波形，因此后续仍需计算峰峰值、普通 RMS、上升时间或显示原始波形时，应先完成这些操作或保留数据副本。

### 相干增益与功率增益

`Hann_getCoherentGainQ15()` 返回窗口系数平均值，用于修正正弦信号的 FFT 幅值。长 Hann 窗的相干增益接近二分之一，但当前模块根据实际长度和量化后的系数计算，不能简单写死。

`Hann_getPowerGainQ30()` 返回系数平方平均值，用于修正噪声功率、功率谱和加窗后的能量统计。幅值修正与功率修正使用的增益不同，不能混用。

## 基本使用方式

以当前512帧 ADC 数据块为例，窗口对象和系数数组应定义为静态变量，避免占用任务栈。

```c
#include "Algorithms/preprocessing/window_functions/hann/hann.h"

#define FFT_LENGTH  (ADC_MULTI_FRAME_COUNT)

static int16_t gHannCoefficients[FFT_LENGTH];
static Hann_Window gHannWindow;

static bool Spectrum_init(void)
{
    return Hann_init(
        &gHannWindow, gHannCoefficients, FFT_LENGTH);
}
```

初始化完成后，每个数据块只调用 `Hann_apply()`：

```c
static int16_t gFFTInput[FFT_LENGTH];

/* gFFTInput 已完成校准和去直流。 */
if (!Hann_apply(&gHannWindow, gFFTInput, gFFTInput)) {
    /* 窗口或数据指针无效。 */
}

/* 接下来把 gFFTInput 送入同长度的 Q15 FFT。 */
```

## 与 ADC 数据链结合

推荐处理顺序为：

```text
ADC原始码
→ ADC校准
→ 保存需要的时域测量结果
→ DC Removal
→ Hann加窗
→ FFT
→ 相干增益修正
→ 基波、谐波和THD
```

一路 `vin` 的数据整理示例：

```c
static int16_t gVinQ15[FFT_LENGTH];
static int32_t gRemovedDC;

for (index = 0U; index < frameCount; index++) {
    gVinQ15[index] = ADCCalibration_applyToInt16(
        &gVinCalibration, frames[index].vin);
}
ADCMulti_releaseBuffer(frames);

/* 时域峰峰值、原始RMS等测量应放在这里。 */

if (DCRemoval_processInt16(
        gVinQ15, gVinQ15, frameCount, &gRemovedDC) &&
    (frameCount == Hann_getLength(&gHannWindow))) {
    Hann_apply(&gHannWindow, gVinQ15, gVinQ15);
    /* 调用同长度FFT。 */
}
```

四个 ADC 通道可以共享同一个只读 Hann 窗对象和系数数组，因为窗口不保存运行状态。但每路待加窗的数据数组必须独立，不能在 FFT 尚未完成时互相覆盖。

## 幅值与功率修正

加窗后的 FFT 幅值不能直接作为真实幅值。完成 FFT 自身的级缩放和单边谱修正后，还要除以相干增益。建议后续频谱模块统一完成修正，避免每个应用重复处理定点比例。

功率谱、噪声功率或加窗数据的平方和应使用功率增益修正。相干增益约为二分之一，功率增益约为八分之三，但实际代码应读取窗口对象中的值，而不是使用理论常数。

## 适用场景

Hann 窗适合：

- 一般频谱显示；
- 基波频率和幅值估计；
- 谐波搜索和 THD；
- 数据块不能保证包含整数周期的情况；
- 需要比矩形窗更低频谱泄漏的测量。

Hann 窗不特别适合：

- 分辨间隔非常近的两个频率；
- 追求最高幅值平坦度的仪表测量；
- 直接进行时域峰值、边沿和瞬态分析。

幅值准确度优先时可使用 Flat-top 窗；确定数据块严格包含整数周期且需要最高频率分辨率时，可以不加窗，相当于使用矩形窗。

## 注意事项

- 窗口长度必须与数据长度和 FFT 长度完全一致。
- `Hann_init()` 只需调用一次，系数缓冲区在使用期间不能释放或改写。
- 原地加窗会永久覆盖原始数据，应先完成需要原波形的测量。
- 加窗不能替代 DC Removal，较大的直流分量仍会影响低频频谱。
- Hann 会降低幅值并拓宽主瓣，FFT 后必须进行正确增益修正。
- 相干增益用于正弦幅值，功率增益用于功率和噪声，两者不能混用。
- Q15 中的一无法精确表示，峰值系数为32767，会产生极小的定点衰减。
- 四通道可共享窗口系数，但不能共享正在处理的数据缓冲区。
