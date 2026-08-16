# FFT 原理与使用方式

## 原理

FFT 用来把一段 ADC 时域采样转换成频谱。时域数组表示电压随采样时刻的变化，FFT 输出数组中的不同位置则代表不同频率。当前模块实现基2正向 FFT，所以点数必须是2的整数次幂，支持16～1024点。

FFT 最关键的操作是蝶形运算。它每次取上、下两个复数，先把下面的数据乘以旋转因子，再分别与上面的数据相加、相减：

```text
旋转后的下方数据 = 下方数据 × 旋转因子
新上方数据       = (上方数据 + 旋转后的下方数据) / 2
新下方数据       = (上方数据 - 旋转后的下方数据) / 2
```

相加倾向于保留两点中变化方向相同的部分，相减倾向于保留变化方向相反的部分；旋转因子负责处理两个信号之间不只是同相、反相的其他相位差。算法先做2点蝶形，再依次组合成4点、8点，直到全部采样点被组合。这样便把不同变化速度的成分逐步分到不同频点，而不必用普通 DFT 对每个输入、输出逐一相乘。

## 实数输入为什么变成复数

ADC 数据是实数。装载数据时，模块只是把 ADC 值放入复数实部，并把虚部设为零：

```text
100   → (100, 0)
-200  → (-200, 0)
```

这一步由 `FFT_loadReal()` 完成。虚部不是 ADC 测出来的，而是在后续数据乘旋转因子时产生的。频谱需要同时用实部和虚部表示一个频率的幅值与相位，所以 FFT 内部统一使用 `FFT_ComplexQ15`：

```c
typedef struct {
    int16_t real;
    int16_t imag;
} FFT_ComplexQ15;
```

不能只用输出实部判断某个频率是否存在。该频点的强弱同时取决于实部和虚部，通常根据两者平方和计算复数模值。

## 关键处理步骤

### `FFT_init()`

该函数检查 FFT 长度、计算级数，并在调用者提供的缓冲区中生成旋转因子。旋转因子相当于蝶形运算需要使用的一组固定旋转角度。模块使用定点 CORDIC 生成它们，不依赖浮点 `sin()`、`cos()`。

旋转因子只与 FFT 长度有关，所以应在系统启动时或 FFT 长度改变时初始化一次，不能每收到一个 ADC 数据块就重新初始化。

### `FFT_reverseBits()` 与位倒序

在蝶形开始前，`FFT_execute()` 会把输入按二进制下标倒序重新排列。例如8点数据中的下标1是二进制 `001`，倒序后是 `100`，因此移动到下标4。这个排列使后面的2点、4点、8点蝶形可以按照连续的数组位置逐级进行。最终输出已经是正常频点顺序，调用者不需要再次倒序。

### `FFT_execute()` 与蝶形

`FFT_execute()` 是核心函数。每一级都遍历所有数据组，对组内成对的数据执行复数旋转、相加和相减。2点蝶形完成后，结果继续进入4点蝶形，直到完成 N 点组合。

代码每一级都将蝶形结果除以2，避免两个接近 Q15 满量程的数相加后溢出。N点 FFT 一共有 `log2(N)` 级，因此最终结果等于普通 DFT 结果除以 N。例如1024点 FFT 有10级，`FFT_ExecutionInfo.scaleShift` 返回10。

### 饱和检测

每次结果写回 `int16_t` 前都会进行饱和处理。若 `FFT_ExecutionInfo.saturationCount` 不为零，说明至少一个中间结果超出 Q15 范围并被截断，此时频谱可能失真，应减小输入幅值或提前增加缩放余量。

## 基本使用方式

下面以512点 ADC 数据为例。FFT 工作区和旋转因子数组较大，应定义为静态或全局变量，不要放在函数局部栈中。

```c
#include "Algorithms/transforms/fft/fft.h"

#define FFT_LENGTH  (512U)

static FFT_PlanQ15 gFFTPlan;
static FFT_ComplexQ15 gFFTTwiddles[FFT_LENGTH / 2U];
static FFT_ComplexQ15 gFFTOutput[FFT_LENGTH];
static int16_t gFFTInput[FFT_LENGTH];

static bool Spectrum_init(void)
{
    return FFT_init(
        &gFFTPlan,
        FFT_LENGTH,
        gFFTTwiddles,
        FFT_LENGTH / 2U);
}
```

每得到一帧已经转换为有符号 Q15 的 ADC 数据后调用：

```c
static bool Spectrum_process(void)
{
    FFT_ExecutionInfo info;

    if (!FFT_executeReal(
            &gFFTPlan, gFFTInput, gFFTOutput, &info)) {
        return false;
    }

    if (info.saturationCount != 0U) {
        /* 输入余量不足，本帧频谱可能失真。 */
        return false;
    }

    /* gFFTOutput 现在保存完整复数频谱。 */
    return true;
}
```

`FFT_executeReal()` 是最适合 ADC 数据的接口，内部先调用 `FFT_loadReal()` 把实数装入复数工作区，再调用 `FFT_execute()`。如果数据本来就是复数，则可以直接准备 `FFT_ComplexQ15` 数组并调用 `FFT_execute()` 原地变换。

## 与 ADC 数据链结合

推荐处理顺序：

```text
ADC原始码
→ ADC校准并转换为有符号数据
→ 必要的时域测量
→ DC Removal
→ Hann或Flat-top加窗
→ FFT_executeReal()
→ 复数模值与单边谱处理
→ 窗增益补偿
→ 频率、幅值或谐波分析
```

假设 ADC 驱动返回的数据块长度恰好等于 FFT 长度，可以先复制并完成校准，然后释放 ADC 缓冲区：

```c
static void ProcessADCFrame(
    const ADCMulti_Frame *frames, uint16_t frameCount)
{
    uint16_t index;

    if (frameCount != FFT_LENGTH) {
        return;
    }

    for (index = 0U; index < frameCount; index++) {
        /* 示例只表示数据流。这里应使用实际校准和Q15转换函数。 */
        gFFTInput[index] = ConvertVinToQ15(frames[index].vin);
    }

    /* 接下来完成去直流、加窗，再调用 Spectrum_process()。 */
}
```

ADC 原始码通常是无符号数并带有中点偏置，不能直接强制转换成 `int16_t` 交给 FFT。必须先扣除零偏并根据量程缩放为有符号 Q15，否则直流频点会很大，交流频谱也可能解释错误。

## 频点如何对应实际频率

设采样率为 `sampleRateHz`，FFT 长度为 `FFT_LENGTH`，第 `bin` 个频点对应：

```text
frequencyHz = bin × sampleRateHz / FFT_LENGTH
```

例如采样率为51200 Hz、FFT长度为512，则频点间隔是100 Hz，第10个频点对应1000 Hz。

实数输入的频谱左右对称，通常只处理 `0～N/2`。可以用下面的接口取得有效单边频点数量：

```c
uint16_t binCount = FFT_getRealBinCount(&gFFTPlan);
```

512点 FFT 返回257，其中0号是直流，256号是采样率一半。寻找最强交流频率时，可以比较实部、虚部的平方和，不必先计算开方：

```c
uint16_t FindPeakBin(void)
{
    uint16_t bin;
    uint16_t peakBin = 1U;
    int64_t peakPower = -1;
    uint16_t binCount = FFT_getRealBinCount(&gFFTPlan);

    for (bin = 1U; bin + 1U < binCount; bin++) {
        int32_t real = gFFTOutput[bin].real;
        int32_t imag = gFFTOutput[bin].imag;
        int64_t power = (int64_t) real * real +
                        (int64_t) imag * imag;

        if (power > peakPower) {
            peakPower = power;
            peakBin = bin;
        }
    }
    return peakBin;
}
```

这个示例跳过直流点和奈奎斯特点，适合寻找一般交流信号的主频。实际频率仍需根据采样率换算。

## 幅值解释

当前 FFT 每一级除以2，因此输出已经包含除以 N 的缩放，不要在后处理时再次盲目除以 N。对于恰好落在频点上的实数正弦，能量分别出现在正、负两个对称频点；制作单边幅值谱时，中间频点的复数模值通常需要乘2，直流点和奈奎斯特点不能乘2。

如果 FFT 前使用了 Hann 或 Flat-top 窗，还必须使用对应窗口的相干增益修正正弦幅值。窗函数的功率增益只用于功率或噪声修正，不能替代相干增益。

## 接口与内存注意事项

- FFT 长度只支持16～1024范围内的2次幂；推荐先用 `FFT_isLengthSupported()` 检查可配置长度。
- 旋转因子缓冲区至少需要 `FFT_getRequiredTwiddleCount(length)` 个复数，即 N/2 个。
- 实数输入需要 N 个 `int16_t`，复数工作区需要 N 个 `FFT_ComplexQ15`；1024点时两者分别占2048字节和4096字节。
- `FFT_loadReal()` 和 `FFT_executeReal()` 的实数输入、复数输出缓冲区不能重叠。
- `FFT_execute()` 是原地计算，会覆盖复数输入。需要保留原数据时必须提前复制。
- `FFT_init()` 成功后，旋转因子数组在计划使用期间不能被释放、覆盖或修改。
- 同一 FFT 计划和只读旋转因子可以由多个 ADC 通道顺序复用，但同时处理时每路必须拥有独立工作区。
- 输入、加窗长度和 FFT 长度必须完全一致；不足时不能直接读取数组外数据，过长时应明确分帧。
- FFT 只输出复数频谱，不负责计算模值、单边谱、dB、窗增益补偿和 ADC 电压换算，这些应由后续频谱模块统一处理。
- 不加窗只适合采样块恰好包含整数个信号周期的情况。一般频谱和谐波分析优先使用 Hann，准确测量单个正弦幅值可考虑 Flat-top。
