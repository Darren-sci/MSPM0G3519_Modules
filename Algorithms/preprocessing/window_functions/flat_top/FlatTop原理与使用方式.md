# Flat-top 原理与使用方式

## 原理

FFT 的频率只能落在一系列离散频点上。实际正弦频率如果位于两个频点之间，能量会分散到相邻频点，最高谱线的幅值低于信号真实幅值。Hann 窗能减少远处频谱泄漏，但最高谱线仍会随着频率在频点之间移动而产生较明显变化。

Flat-top 窗通过五个余弦分量组合出顶部较平坦的频率响应主瓣。正弦频率在一个 FFT 频点附近偏移时，主瓣最高值变化较小，因此更适合测量正弦幅值。它的代价是主瓣明显变宽，两个距离较近的频率更容易重叠，频率分辨能力弱于 Hann 窗。

当前实现采用标准五项对称 Flat-top 窗。时域边缘系数会出现很小的负值，这是标准窗形的一部分，不是溢出或生成错误。系数最终保存为 Q15，内部余弦和五项组合使用更高精度定点运算。

## 关键函数

### `FlatTop_cosineFirstQuadrantQ30()`

五项 Flat-top 窗需要一倍角到四倍角的余弦。Cortex-M0+ 没有硬件浮点，直接调用 `cos()` 会带来软件浮点和数学库开销，因此模块使用31轮 Q30 CORDIC。

CORDIC 从一个经过增益补偿的向量开始，每轮根据剩余角度选择旋转方向，只使用加法、减法、移位和固定角度表。最终横坐标就是 Q30 余弦值。底层只计算第一象限，以缩小角度范围并保持数值稳定。

### `FlatTop_cosineQ30()`

该函数把完整32位相位划分为四个象限，再利用余弦对称性映射到第一象限。32位相位的一整圈会自然溢出回零，因此计算二倍角、三倍角和四倍角时可以直接进行无符号乘法，无需浮点取模。

### `FlatTop_coefficientQ15()`

该函数把常数项与四个余弦项按照交替符号组合。标准五项系数使用 Q30 保存，余弦也是 Q30，两者相乘后用六十四位变量累加。最终结果经过正负对称四舍五入、缩放和饱和，得到 Q15 窗系数。

Flat-top 中心权重接近一，边缘权重约为很小的负数。Q15量化后，当前标准系数的边缘通常约为 `-14`。

### `FlatTop_init()`

`FlatTop_init()` 根据指定长度生成系数，并计算相干增益和功率增益。调用者必须提供与窗口等长的 `int16_t` 系数数组。函数只生成前半窗，再镜像到后半窗，从而减少计算并保证定点舍入后仍然严格对称。

Flat-top 每个位置需要进行多次 CORDIC 运算，初始化明显比 Hann 慢。它应在系统启动时执行一次，不能在每个 DMA 数据块中重新生成。

### `FlatTop_apply()`

`FlatTop_apply()` 将每个 Q15 输入与对应 Q15 系数相乘，四舍五入后输出 Q15。输入和输出可以是不同数组，也可以指向同一数组。原地处理会覆盖原始波形，需要保留时域数据时必须提前复制或先完成时域测量。

### 相干增益与功率增益

`FlatTop_getCoherentGainQ15()` 返回实际量化窗口系数的平均值，用于正弦 FFT 幅值修正。Flat-top 相干增益约为0.216，远小于 Hann，因此未经修正的频谱幅值会明显降低。

`FlatTop_getPowerGainQ30()` 返回窗口系数平方平均值，用于功率谱、噪声功率和能量修正。相干增益和功率增益用途不同，不能互相替代。

## 基本使用方式

以当前512点 ADC 数据块为例，窗口对象和系数数组应定义为静态变量。

```c
#include "Algorithms/preprocessing/window_functions/flat_top/flat_top.h"

#define FFT_LENGTH  (ADC_MULTI_FRAME_COUNT)

static int16_t gFlatTopCoefficients[FFT_LENGTH];
static FlatTop_Window gFlatTopWindow;

static bool AmplitudeSpectrum_init(void)
{
    return FlatTop_init(
        &gFlatTopWindow,
        gFlatTopCoefficients,
        FFT_LENGTH);
}
```

初始化完成后，每个数据块只进行加窗：

```c
static int16_t gFFTInput[FFT_LENGTH];

/* gFFTInput 已经完成校准和去直流。 */
if (!FlatTop_apply(
    &gFlatTopWindow, gFFTInput, gFFTInput)) {
    /* 窗口对象或数据指针无效。 */
}

/* 接下来调用同长度的 Q15 FFT。 */
```

## 与 ADC 数据链结合

推荐处理顺序：

```text
ADC原始码
→ ADC校准
→ 时域参数测量
→ DC Removal
→ Flat-top加窗
→ FFT
→ FFT自身缩放修正
→ Flat-top相干增益修正
→ 正弦幅值
```

一路 `vin` 的数据准备示例：

```c
static int16_t gVinQ15[FFT_LENGTH];
static int32_t gRemovedDC;

for (index = 0U; index < frameCount; index++) {
    gVinQ15[index] = ADCCalibration_applyToInt16(
        &gVinCalibration, frames[index].vin);
}
ADCMulti_releaseBuffer(frames);

/* 峰峰值、普通RMS和原始波形显示应放在加窗之前。 */

if (DCRemoval_processInt16(
        gVinQ15, gVinQ15, frameCount, &gRemovedDC) &&
    (frameCount == FlatTop_getLength(&gFlatTopWindow))) {
    FlatTop_apply(&gFlatTopWindow, gVinQ15, gVinQ15);
    /* 调用同长度 FFT。 */
}
```

Flat-top 窗没有运行状态，四个通道可以共享同一窗口对象和只读系数数组。但每路数据缓冲区和 FFT 工作区必须独立使用，前一路 FFT 完成前不能被下一路覆盖。

## 幅值修正

加窗后的 FFT 谱线必须先根据 FFT 实现恢复级缩放和单边谱系数，再除以 Flat-top 相干增益。修正顺序和定点尺度应由后续频谱模块统一管理，避免应用层重复换算或发生六十四位溢出。

不能简单地把频谱值乘以某个固定常数，因为对称窗的实际增益会随长度略微变化，Q15量化也会引入细小差异。应读取：

```c
uint16_t coherentGain =
    FlatTop_getCoherentGainQ15(&gFlatTopWindow);
```

噪声功率或功率谱则读取：

```c
uint32_t powerGain =
    FlatTop_getPowerGainQ30(&gFlatTopWindow);
```

## 与 Hann 窗的选择

优先使用 Flat-top 的情况：

- 测量单个或少量正弦信号的准确幅值；
- 测量放大器增益；
- 扫频测试中比较输入和输出幅值；
- 信号不能保证落在整数 FFT 频点；
- 电子电压表或频谱幅值仪表。

优先使用 Hann 的情况：

- 一般频谱显示；
- 搜索基波频率；
- 分辨距离较近的频率；
- 谐波和 THD 分析；
- 更关注泄漏与分辨率之间的通用折中。

若数据块严格包含整数个周期且需要最窄主瓣，可以不加窗，相当于矩形窗。但比赛现场通常难以保证信号和采样时钟严格同步。

## 注意事项

- 窗口长度必须与输入数据和 FFT 长度完全一致。
- `FlatTop_init()` 计算量较大，只应在初始化或改变FFT长度时调用。
- 系数缓冲区由调用者管理，在窗口使用期间不能释放或改写。
- Flat-top 边缘小负系数是正常现象，不要强制改成零，否则会改变窗的幅值平坦度。
- 原地加窗会覆盖原始波形，时域峰值、普通RMS、边沿和波形显示应提前完成。
- Flat-top 主瓣很宽，不适合分辨相邻频率，也可能使相邻谐波主瓣重叠。
- FFT幅值必须使用相干增益修正，功率和噪声必须使用功率增益修正。
- Flat-top 不能替代 ADC 增益校准、DC Removal、FFT定点缩放和削顶检测。
- 输入接近 Q15 满量程时，应为后续 FFT 运算保留足够余量。
- 四通道可以共享窗口系数，但不能共享正在处理的数据或 FFT 工作区。
