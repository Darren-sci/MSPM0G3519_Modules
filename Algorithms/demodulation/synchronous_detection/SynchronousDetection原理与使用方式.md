# Synchronous Detection 原理与使用方式

## 原理

同步检波只提取与参考信号同频的成分。被测信号分别乘以 PLL 产生的同相正弦和正交余弦，再进行平均或低通：

```text
I = 低通(输入 × 参考正弦) × 2
Q = 低通(输入 × 参考余弦) × 2
```

若输入相对于参考的相位为0度，结果主要落在 I；相位为+90度时主要落在 Q。最终幅值由 I、Q 的平方和开方得到，相位由 `atan2(Q,I)` 得到。

与整流包络不同，同步检波能够区分正负相位，并且不与参考同频的噪声在长时间平均中会互相抵消，所以特别适合弱信号、阻抗和扫频测量。

乘法结果中的目标直流量只有原正弦峰值的一半，因此模块内部乘2恢复峰值。输入使用 mV 时，I、Q和幅值仍是 mV；输入使用 mA 时仍是 mA。

## 两种工作方式

`SynchronousDetection_calculateBlock()` 对完整数据块求和后平均，适合信号稳定且数据块覆盖整数个或足够多个周期的情况。它没有跨块状态，调用简单。

`SynchronousDetection_processSample()` 使用一阶连续低通，状态跨DMA块保留，适合PLL实时跟踪、扫频和连续仪表。低通系数越小，抗噪声和二倍频抑制越好，但稳定时间越长。

## 与 PLL 配合

```c
static SynchronousDetection_State gDetector;
static SynchronousDetection_Result gDetection;

static bool Detector_init(void)
{
    return SynchronousDetection_init(
        &gDetector,
        128U,     /* 低通系数Q15 */
        100U,     /* 最小有效幅值 */
        2000U);   /* 至少处理2000点才稳定 */
}

static bool Detector_process(int32_t measuredValue,
    const PLL_Output *pllOutput)
{
    if (!pllOutput->locked) {
        SynchronousDetection_reset(&gDetector);
        return false;
    }

    return SynchronousDetection_processSample(
        &gDetector,
        measuredValue,
        pllOutput->sineQ15,
        pllOutput->cosineQ15,
        &gDetection);
}
```

正式读取结果时应同时满足：

```c
pllOutput->locked && gDetection.valid && gDetection.stable
```

PLL刚锁定时，同步检波低通还没有稳定，需要继续等待。若PLL失锁或切换频率，应重置同步检波器。

## 数据块相干积分

```c
SynchronousDetection_Result result;

if (SynchronousDetection_calculateBlock(
        measuredMilliVolt,
        pllSineQ15,
        pllCosineQ15,
        sampleCount,
        &result)) {
    /* result.amplitude：mV峰值 */
    /* result.phaseMilliDegrees：相对于PLL参考的相位 */
}
```

数据块最好包含整数个参考周期。非整数周期时，二倍频项和直流偏置不能完全抵消，可以延长积分时间或改用连续低通。

## 注意事项

- 输入、正弦参考、余弦参考必须逐点同步，长度完全相同。
- PLL正弦定义为同相，余弦超前90度，因此正Q表示被测信号相位超前参考。
- 被测信号应先校准为物理单位；不要为每帧自动归一化。
- 大直流偏置会通过有限积分泄漏到结果，必要时使用连续高通或准确零点校准。
- 多频信号中只有目标参考附近成分被保留，但有限低通带宽仍可能通过邻近频率。
- `saturated=true` 表示恢复后的I/Q超出32位，结果不能继续用于阻抗或频响计算。
- 连续模式的 `stable` 只是等待时间和最小幅值判断，不代替PLL锁定与系统校准。
- `atan2` 使用定点CORDIC，输出单位为0.001度。

