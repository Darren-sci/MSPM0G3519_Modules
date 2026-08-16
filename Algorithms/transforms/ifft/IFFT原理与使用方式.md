# IFFT 原理与使用方式

## 原理

FFT 把一段时域数据拆成不同频率的复数分量，IFFT 则执行相反过程：把所有频率分量按照各自的幅值和相位重新叠加，恢复为时域数据。

可以把 FFT 理解成把一杯混合饮料分辨出其中有哪些成分，IFFT 则根据这些成分重新混合出饮料。IFFT 不是只把数组顺序倒过来，而是重新执行多级蝶形运算，只是旋转方向与正向 FFT 相反。

## 最关键的蝶形逻辑

IFFT 每次取上、下两个复数。先用反方向旋转因子旋转下方数据，再进行相加和相减：

```text
旋转后的下方数据 = 下方数据 × 反方向旋转因子
新上方数据       = 上方数据 + 旋转后的下方数据
新下方数据       = 上方数据 - 旋转后的下方数据
```

FFT 中的旋转方向可理解为顺时针，IFFT 则改为逆时针。正向 FFT 通过这些旋转和加减把频率分开；IFFT 用相反旋转把分开的频率重新对齐、叠加。算法同样从2点蝶形开始，再组合成4点、8点，直到完成整个 N 点逆变换。

## 为什么复用 FFT 旋转因子

正向和逆向旋转因子的实部相同，虚部符号相反。FFT 计划中已经保存了正向旋转因子，因此 IFFT 不需要再生成或保存一份系数，只需在复数乘法时反转虚部方向。

所以本模块没有单独的 `IFFT_init()`，而是直接接收已经成功初始化的 `FFT_PlanQ15`。这可以减少一份 N/2 长度的旋转因子数组，也保证 FFT 和 IFFT 使用完全相同的长度。

## 本项目的缩放关系

当前 `FFT_execute()` 为防止 Q15 蝶形加法溢出，每一级都会把结果除以2。N点 FFT 一共有 `log2(N)` 级，最终频谱等于普通 DFT 结果除以 N。

IFFT 的输入就是这种已经除以 N 的频谱，所以 `IFFT_execute()` 不再逐级除以2。每一级直接相加、相减，累计放大 N 倍，最终恢复原时域尺度：

```text
原时域数据
→ FFT内部累计除以N
→ 归一化频谱
→ IFFT内部累计放大N倍
→ 恢复时域数据
```

如果把其他 FFT 库产生的、没有除以 N 的普通 DFT 频谱直接传入，本模块会再放大 N 倍，输出很可能饱和。因此 `IFFT_execute()` 默认只与本项目的 `FFT_execute()` 配套使用。

## 为什么需要32位工作区

FFT 输出是 Q15 复数，每个实部或虚部占 `int16_t`。IFFT 不进行逐级缩小，中间数据会随着蝶形逐级增大。如果直接在 Q15 数组中计算，即使最终恢复值没有超限，中间结果也可能提前被截断。

因此模块先把频谱复制到 `IFFT_ComplexI32` 工作区：

```c
typedef struct {
    int32_t real;
    int32_t imag;
} IFFT_ComplexI32;
```

所有蝶形先在32位工作区中完成，最后才饱和转换回 `FFT_ComplexQ15`。1024点 IFFT 需要8192字节工作区。工作区应定义为静态或全局数组，不能放在空间较小的函数栈中。

## 关键函数

### `IFFT_getRequiredWorkspaceCount()`

返回所需的 `IFFT_ComplexI32` 元素数量，它与 FFT 长度相同。FFT 计划无效或尚未初始化时返回0。

### `IFFT_reverseBits()`

IFFT 和当前基2 FFT 一样，在蝶形前进行位倒序排列，使后面的2点、4点、8点蝶形可以按连续数组位置执行。最终输出已经恢复为正常时间顺序。

### `IFFT_execute()`

这是对外使用的核心接口，依次完成：

```text
检查FFT计划和缓冲区
→ 把Q15频谱复制到32位工作区
→ 位倒序
→ 逐级执行反方向蝶形
→ 转换回Q15复数时域数据
→ 返回饱和信息
```

`spectrum` 和 `output` 可以指向同一个复数数组。函数会先把全部频谱复制到工作区，之后才写输出，所以允许原地覆盖频谱。但32位工作区不能与输入或输出重叠。

## 基本使用方式

FFT 和 IFFT 共用同一个 FFT 计划与旋转因子：

```c
#include "Algorithms/transforms/fft/fft.h"
#include "Algorithms/transforms/ifft/ifft.h"

#define FFT_LENGTH  (512U)

static FFT_PlanQ15 gFFTPlan;
static FFT_ComplexQ15 gTwiddles[FFT_LENGTH / 2U];
static FFT_ComplexQ15 gSpectrum[FFT_LENGTH];
static IFFT_ComplexI32 gIFFTWorkspace[FFT_LENGTH];
static int16_t gTimeInput[FFT_LENGTH];

static bool Transforms_init(void)
{
    return FFT_init(
        &gFFTPlan,
        FFT_LENGTH,
        gTwiddles,
        FFT_LENGTH / 2U);
}
```

先执行 FFT，再原地执行 IFFT：

```c
static bool FFT_IFFT_process(void)
{
    FFT_ExecutionInfo fftInfo;
    IFFT_ExecutionInfo ifftInfo;

    if (!FFT_executeReal(
            &gFFTPlan, gTimeInput, gSpectrum, &fftInfo)) {
        return false;
    }

    /* 可以在这里读取或修改 gSpectrum。 */

    if (!IFFT_execute(
            &gFFTPlan,
            gSpectrum,
            gIFFTWorkspace,
            gSpectrum,
            &ifftInfo)) {
        return false;
    }

    if ((ifftInfo.internalSaturationCount != 0U) ||
        (ifftInfo.outputSaturationCount != 0U)) {
        /* 频谱尺度错误或数据超出可恢复范围。 */
        return false;
    }

    /* gSpectrum[index].real 是恢复后的实数时域数据。 */
    return true;
}
```

由于 FFT 使用定点乘法并且每一级都需要舍入，FFT 后再 IFFT 的结果不保证与输入每一位完全相同。通常会出现少量实部误差和很小的虚部残留，这是定点量化造成的正常现象。

## 修改频谱后再恢复时域

IFFT 常用于频域处理。例如将某些频点清零，再恢复时域，相当于在频域完成滤波：

```c
static bool ClearBins(uint16_t firstBin, uint16_t lastBin)
{
    uint16_t bin;

    if ((firstBin > lastBin) ||
        (lastBin > FFT_LENGTH / 2U)) {
        return false;
    }

    for (bin = firstBin; bin <= lastBin; bin++) {
        uint16_t mirror = (uint16_t) (FFT_LENGTH - bin);

        gSpectrum[bin].real = 0;
        gSpectrum[bin].imag = 0;

        if ((bin != 0U) && (bin != FFT_LENGTH / 2U)) {
            gSpectrum[mirror].real = 0;
            gSpectrum[mirror].imag = 0;
        }
    }
    return true;
}
```

实数时域信号的频谱具有共轭对称性。修改一个正频率时，必须同步修改对应的负频率镜像，否则 IFFT 输出会出现明显虚部，不能再视为纯实数信号。0号直流点和 N/2 奈奎斯特点没有独立镜像，不应按普通频点重复处理。

如果 FFT 前使用了 Hann 或 Flat-top 窗，IFFT 恢复的是“加窗后的时域波形”，不会自动恢复被窗口压低的首尾数据。窗函数边缘接近零时，简单除以窗系数还会严重放大误差，所以不能把 FFT→IFFT 当成自动撤销窗函数的方法。

## 从复数输出取得实数

若原始输入为实数并且频谱保持共轭对称，IFFT 输出虚部应接近零。可以在检查虚部残留后提取实部：

```c
static int16_t gRecovered[FFT_LENGTH];

static bool ExtractRealOutput(int16_t allowedImaginaryError)
{
    uint16_t index;

    for (index = 0U; index < FFT_LENGTH; index++) {
        int32_t imag = gSpectrum[index].imag;

        if (imag < 0) {
            imag = -imag;
        }
        if (imag > allowedImaginaryError) {
            return false;
        }
        gRecovered[index] = gSpectrum[index].real;
    }
    return true;
}
```

允许的虚部误差应根据 FFT 长度、输入幅值和实际测试结果确定，不能要求定点往返后严格等于零。

## 注意事项

- 必须先成功初始化 `FFT_PlanQ15`，IFFT 本身不需要单独初始化。
- IFFT 长度由 FFT 计划决定，频谱、输出和工作区都必须与该长度一致。
- 频谱输入应来自本项目同长度的 `FFT_execute()`，其尺度已经是 DFT/N。
- `spectrum` 与 `output` 可以相同；`workspace` 不能与它们发生任何内存重叠。
- IFFT 会覆盖输出数组，原地调用时原频谱无法保留，需要时应提前复制。
- 1024点工作区占8192字节，应确认链接文件和全局RAM仍有足够空间。
- `internalSaturationCount` 非零表示32位中间结果溢出，`outputSaturationCount` 非零表示最终时域结果超过 Q15。
- 对实数信号修改频谱时必须维持正、负频率的共轭对称关系。
- IFFT 不会撤销 ADC 校准、DC Removal、滤波或窗函数；它只把当前频谱重新合成为时域数据。
- FFT→IFFT 会有定点量化误差，不适合要求逐位无损恢复的场合。
- 若只做频率、幅值、THD 或频谱显示，一般不需要 IFFT；只有需要频域修改后返回时域、频域滤波或波形重建时才使用。
