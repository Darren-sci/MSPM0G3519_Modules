# Overrange Detection 原理与使用方式

## 原理

超量程检测用于判断ADC或校准后的信号是否过于接近上下限。单个异常点可能只是噪声，所以模块要求连续多个点越界后才置位；恢复时又要求信号连续回到带迟滞的安全区域，避免状态在边界附近反复跳变。

例如上限30000、释放迟滞1000时：

```text
sample >= 30000：累计高端越界
sample <= 29000：累计高端释放
29000～30000：保持原状态
```

状态跨数据块保留，所以前一DMA块末尾两个削顶点加下一块开头一个削顶点，也能构成连续三个点。

## 使用方式

```c
static OverrangeDetection_State gOverrange;

static const OverrangeDetection_Config gOverrangeConfig = {
    .lowerLimit = -30000,
    .upperLimit = 30000,
    .releaseHysteresis = 1000U,
    .consecutiveAssertSamples = 3U,
    .consecutiveReleaseSamples = 16U
};

static bool Overrange_init(void)
{
    return OverrangeDetection_init(
        &gOverrange, &gOverrangeConfig);
}
```

处理每个校准数据块：

```c
OverrangeDetection_Result result;

if (OverrangeDetection_processBlock(
        &gOverrange, samples, sampleCount, &result)) {
    if (result.lowActive || result.highActive) {
        /* 本通道测量结果可能已经削顶，不应相信RMS、FFT和功率。 */
    }
}
```

结果还提供块内最小值、最大值、上下越界点数、最长连续越界长度和第一次越界位置，便于区分偶发尖峰与持续削顶。

## 阈值选择

不要把阈值直接设成ADC绝对码值极限。模拟前端、校准和数字滤波可能需要余量，例如Q15信号可以在±30000附近预警，而不是等到±32768才报告。

若检测ADC原始码，应使用单极性上下限；若检测去偏置后的Q15，应使用正负对称限值；若检测mV等物理量，应根据量程和前端允许范围设置。

## 注意事项

- 超量程检测应放在会改变峰值的滤波、加窗和归一化之前。
- 一个点越界不一定表示削顶，连续点确认更可靠，但会增加检测延迟。
- `releaseHysteresis` 不能超过上下限之间的范围。
- 上下限同时用于保护测量可信度，不等同于硬件过压、过流保护；危险保护必须由更可靠的硬件或实时路径完成。
- FIR/IIR滤波可能隐藏原始削顶，所以应优先检查原始校准波形。
- 状态跨块保留；切换通道、量程或开始新测量时调用 `reset()`。

