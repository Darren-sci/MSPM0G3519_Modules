# Digital PLL 原理与使用方式

## 模块用途

本模块是处理 ADC 采样信号的软件数字锁相环，不是 MCU 时钟树中的硬件 PLL，也不是电机控制器。它跟踪一个近似正弦输入的频率和相位，并产生与输入同相的 Q15 正弦、超前90度的 Q15 余弦。

主要用途：

- 从 ADC 正弦信号恢复频率和连续相位；
- 为同步检波、锁相放大器产生同相和正交参考；
- 跟踪扫频信号或缓慢漂移的激励源；
- 产生连续 DAC 参考波形；
- 阻抗、增益和相位测量中的参考同步。

## 原理

数字 PLL 由三部分组成：数控振荡器、相位检测器和环路滤波器。

```text
ADC输入
   ↓
相位检测器 ← NCO正交参考
   ↓
低通后的相位误差
   ↓
PI环路滤波器
   ↓
修正NCO频率和相位
   └──────────────反馈
```

### 数控振荡器 NCO

NCO 使用32位无符号相位累加器。相位从0增加到最大值后自然溢出回0，正好表示转过一整周。每处理一个采样，相位增加一个频率控制字：控制字越大，输出正弦转得越快。

模块用65点四分之一周期 Q15 表和线性插值产生正弦，其余象限利用对称性得到。这样只占很少ROM，每个采样也不需要调用浮点 `sin()`、`cos()`。

### 相位检测

输入按 `sin(inputPhase)` 理解。NCO 同时产生正弦和余弦，输入乘以正交余弦后包含两部分：一部分是输入与NCO的相位差，另一部分是约两倍输入频率的高频项。

一阶低通滤掉大部分二倍频项后，剩下的正负值表示NCO落后还是超前：输入相位领先时，环路提高NCO速度；输入相位落后时，环路降低NCO速度。锁定后平均相位误差接近零，NCO正弦与输入同相。

### PI环路滤波器

比例项立即修正本次相位步进，使相位更快靠近输入；积分项持续修改NCO中心频率，消除稳态频率误差。这里的 PI 只是锁相环内部的一部分，与电机 PI/PID 没有关系。

比例太小会捕获慢，太大则输出相位抖动；积分太小无法快速追踪频率漂移，太大容易振荡并撞到频率上下限。因此参数必须结合采样率、输入幅值、噪声和捕获范围测试。

## 配置参数

所有频率参数使用 mHz：

- `initialFrequencyMilliHz`：NCO启动频率，越接近输入越容易捕获；
- `minimumFrequencyMilliHz`、`maximumFrequencyMilliHz`：允许跟踪范围，也防止噪声把环路拉跑；
- `proportionalGainMilliHz`：满量程相位误差产生的瞬时频率修正强度；
- `integralGainMilliHzPerSample`：满量程误差每个采样改变中心频率的大小；
- `detectorFilterQ15`：相位检测低通系数，越小越平滑但响应越慢；
- `minimumInputAmplitudeQ15`：低于此平均绝对幅值时不能宣告锁定；
- `lockErrorThresholdQ15`：允许的低通相位误差；
- `lockSampleCount`：幅值和误差连续满足条件多少点后才锁定。

## 基准配置示例

下面这组参数已用幅值约12000 Q15的1 kHz正弦验证，可从900 Hz捕获1 kHz，并跟踪到1.1 kHz。它是起始参考，不是所有硬件的通用最优值。

```c
#include "Algorithms/demodulation/pll/pll.h"

static PLL_State gPLL;

static const PLL_Config gPLLConfig = {
    .sampleRateHz = 51200U,
    .initialFrequencyMilliHz = 900000U,
    .minimumFrequencyMilliHz = 500000U,
    .maximumFrequencyMilliHz = 1500000U,
    .proportionalGainMilliHz = 200000U,
    .integralGainMilliHzPerSample = 200U,
    .detectorFilterQ15 = 256U,
    .minimumInputAmplitudeQ15 = 1000U,
    .lockErrorThresholdQ15 = 400U,
    .lockSampleCount = 2048U
};

static bool SignalPLL_init(void)
{
    return PLL_init(&gPLL, &gPLLConfig);
}
```

输入频率范围改变后，必须同步调整初始频率和上下限。最好先用零交叉或 FFT 基波检测得到粗略频率，再把 PLL 初始频率设置在附近；PLL负责连续精细跟踪，不适合盲目搜索整个0～奈奎斯特频段。

## 单点实时调用

```c
static PLL_Output gPLLOutput;

void ADC_sampleCallback(int16_t adcSignalQ15)
{
    if (!PLL_processSample(
            &gPLL, adcSignalQ15, &gPLLOutput)) {
        return;
    }

    if (gPLLOutput.locked) {
        /* sineQ15：与输入同相 */
        /* cosineQ15：比输入超前90度 */
        /* frequencyMilliHz：当前跟踪频率 */
    }
}
```

不要每收到一个 DMA 数据块就调用 `PLL_reset()`。PLL 的相位、频率、低通和积分状态必须跨数据块连续保留。

## ADC 数据块调用

如果后续同步检波需要整块正交参考：

```c
#define FRAME_COUNT  (512U)

static int16_t gInputQ15[FRAME_COUNT];
static int16_t gReferenceSineQ15[FRAME_COUNT];
static int16_t gReferenceCosineQ15[FRAME_COUNT];
static PLL_Output gLastPLLOutput;

static bool TrackFrame(void)
{
    return PLL_processBlock(
        &gPLL,
        gInputQ15,
        gReferenceSineQ15,
        gReferenceCosineQ15,
        FRAME_COUNT,
        &gLastPLLOutput);
}
```

只需要频率和最终锁定状态时，两个参考输出数组都可以传空，只保留 `lastOutput`。只需要同相参考时，余弦数组也可以传空。

## 与同步检波结合

PLL锁定后，对输入或另一路被测信号分别乘同相、正交参考，再低通：

```text
I = 低通(被测信号 × PLL正弦)
Q = 低通(被测信号 × PLL余弦)
```

I和Q可以得到相对于参考的幅值和相位，非常适合阻抗测量、弱信号检测和扫频响应。正式测量应等待 `locked=true`，并额外等待同步检波低通稳定。

## 用于 DAC 输出

`sineQ15` 是有符号 Q15。以12位单极性 DAC 为例，可以增加中点偏置并转换为 DAC 码：

```c
static uint16_t Q15_toDac12(int16_t value)
{
    uint32_t shifted = (uint32_t) ((int32_t) value + 32768);
    return (uint16_t) ((shifted * 4095U + 32767U) / 65535U);
}
```

实际输出还要考虑 DAC 更新率、重建滤波、输出幅值校准和模拟偏置。若只需固定频率 NCO，不需要闭环跟踪，也可以直接维护相位累加器并调用 `PLL_phaseToSineQ15()`。

## 调参顺序

推荐按下面顺序调试：

1. 用 FFT 或频率测量确定输入大致范围，把初始频率放在输入附近；
2. 先限制较窄的最小、最大频率，防止失锁后跑远；
3. 使用较小积分增益，逐渐增加比例增益，观察能否捕获且不明显抖动；
4. 逐渐增加积分增益，使频率漂移能够被跟踪，但不能出现来回摆动；
5. 调整检测低通系数，在二倍频纹波和响应速度之间折中；
6. 最后根据实际噪声设置幅值门限、误差门限和连续锁定点数。

参数变化方向可以粗略理解为：

```text
增大比例/积分增益：捕获和跟踪更快，但抖动、振荡风险增加
减小 detectorFilterQ15：误差更平滑，但锁定和失锁判断更慢
增大 lockSampleCount：锁定更可信，但等待时间更长
缩小频率上下限：更不容易被噪声或谐波拉走
```

## 输入前处理

PLL输入应先完成ADC零点和增益校准，并转换为有符号Q15。较大的直流偏置、削顶、多个强频率和大量噪声都会降低锁定可靠性。

如果需要去直流，连续运行的一阶高通 IIR通常比“每个数据块减去自身平均值”更适合PLL，因为逐块减平均可能在块边界造成不连续。输入含有多个频率时，可以先用带通滤波限制到目标频带。

不要对每一帧独立归一化到满量程。归一化突变会改变相位检测器增益，导致环路参数和锁定门限随数据块变化。

## 注意事项

- 当前实现针对单个占主导地位的近似正弦实数输入，不是任意波形的万能频率计。
- 初始频率必须足够接近目标；需要宽范围搜索时先用FFT或零交叉粗测。
- 频率范围必须低于采样率一半，实际还应留出模拟抗混叠和数字滤波余量。
- 输入和 `PLL_processSample()` 的调用速率必须严格等于配置采样率。
- DMA块之间不能漏样、重复样或重置PLL，否则连续相位会被破坏。
- 输入消失时幅值门限会撤销锁定，但NCO仍保持最后的受限频率继续运行。
- 强谐波可能产生错误锁定，必要时在PLL前增加带通滤波。
- 锁定标志只是算法门限判断，正式仪表还应检查频率是否在预期范围、ADC是否削顶。
- 输出相位受ADC通道延迟、模拟滤波器和采样保持延迟影响；精密相位测量需要系统标定。
- 正弦表和线性插值会产生很小的幅值与相位量化误差，不适合直接替代高纯度模拟信号源而不经过评估。
- PI/PID控制目录暂时可以保持为空；只有题目要求MCU直接闭环稳压、稳流或控制模拟执行器时才需要实现。
