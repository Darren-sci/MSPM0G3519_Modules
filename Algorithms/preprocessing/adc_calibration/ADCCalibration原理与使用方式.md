# ADC Calibration 原理与使用方式

## 原理

ADC 校准把原始码转换成统一的算法数据或整数物理量。模块使用低端、零点和高端三个标定码，规定零点输出为零，并分别计算零点两侧的增益。这样可以修正通道零偏、实际满量程误差以及正负方向增益不一致。低端与高端之外的输入会钳位到端点值，避免异常 ADC 码被继续放大。

关键函数 `ADCCalibration_apply()` 先检查输入位于低端、零点还是高端区间，再调用 `ADCCalibration_interpolate()` 完成对应区间的线性换算。中间乘法和除法使用六十四位整数，并对正负结果进行对称四舍五入。`ADCCalibration_applyToInt16()` 在换算后增加有符号十六位饱和，适合输出 Q15 或较小的整数物理量。校准配置不保存采样历史，因此单点之间互不依赖，也不需要复位函数。

## 使用方式

通用物理量转换使用 `ADCCalibration_init()`，低端和高端输出可以定义为 mV、mA、mW 或其他整数单位。只需要算法归一化数据时，可以使用 `ADCCalibration_initUnipolarQ15()` 或 `ADCCalibration_initBipolarQ15()`。单点转换使用 `ADCCalibration_apply()` 或 `ADCCalibration_applyToInt16()`，连续数组使用相应的块处理接口。

## 标定参数实例

```c
static ADCCalibration_Config gCalibration;

/* 单极性 Q15：ADC 码 24 为零点，4072 为正满量程。 */
ADCCalibration_initUnipolarQ15(&gCalibration, 24U, 4072U);

/* 双极性 Q15：模拟前端把信号偏置在 ADC 码 2051。 */
ADCCalibration_initBipolarQ15(
    &gCalibration, 85U, 2051U, 4018U);

/* ADC 引脚电压：码 18 对应 0 mV，码 4060 对应 3296 mV。 */
ADCCalibration_init(&gCalibration,
    18U, 18U, 4060U, 0, 3296);

/* 外部双极性电压：三个标定点对应 -12000、0、12000 mV。 */
ADCCalibration_init(&gCalibration,
    92U, 2046U, 3995U, -12000, 12000);

/* 反相电流前端：ADC 码增大时，实际电流反而减小。 */
ADCCalibration_init(&gCalibration,
    100U, 2050U, 4000U, 5000, -5000);
```

这些数字只是接口示例，不能当作开发板的真实校准值。正式参数应使用可靠仪表施加已知输入，并对多个 ADC 样本取平均后获得。三个码必须满足低端不大于零点、零点严格小于高端。

## 示例一：在 main 中把一路 ADC 转成毫伏

下例假定模拟前端为单极性输入，实测 ADC 码 20 对应 0 mV，码 4068 对应 ADC 引脚的 3298 mV。

```c
#include "ti_msp_dl_config.h"
#include "Drivers/adc_multi.h"
#include "Algorithms/preprocessing/adc_calibration/adc_calibration.h"

static ADCCalibration_Config gVinCalibration;
static int32_t gVinMillivolts[ADC_MULTI_FRAME_COUNT];

int main(void)
{
    const ADCMulti_Frame *frames;
    uint16_t frameCount;
    uint16_t index;

    SYSCFG_DL_init();
    ADCMulti_init();

    if (!ADCCalibration_init(&gVinCalibration,
        20U, 20U, 4068U, 0, 3298)) {
        while (1) {
        }
    }
    if (!ADCMulti_start(ADC_MULTI_DEFAULT_RATE_HZ)) {
        while (1) {
        }
    }

    while (1) {
        if (ADCMulti_getReadyBuffer(&frames, &frameCount)) {
            for (index = 0U; index < frameCount; index++) {
                gVinMillivolts[index] = ADCCalibration_apply(
                    &gVinCalibration, frames[index].vin);
            }

            ADCMulti_releaseBuffer(frames);
            /* 使用 gVinMillivolts 显示、统计或判断量程。 */
        }
    }
}
```

如果 ADC 前端还有分压器，`highValue` 可以直接填写分压器输入端对应的毫伏数。例如码 4068 实际代表外部 30 V 时，可以填写 30000，之后得到的结果就直接是外部电压毫伏值。

## 示例二：校准为 Q15 后送入 FIR

双极性信号先校准为 Q15，再送入滤波、RMS、FFT 等算法。滤波器状态在 DMA 数据块之间保持连续，校准本身没有历史状态。

```c
static ADCCalibration_Config gVinQ15Calibration;
static int16_t gVinQ15[ADC_MULTI_FRAME_COUNT];
static int16_t gFilteredVin[ADC_MULTI_FRAME_COUNT];

static bool SignalChain_init(void)
{
    if (!ADCCalibration_initBipolarQ15(
        &gVinQ15Calibration, 80U, 2048U, 4015U)) {
        return false;
    }
    return FIR_init(&gVinFilter, gVinCoefficients,
        gVinState, VIN_FIR_TAP_COUNT);
}

static void SignalChain_process(
    const ADCMulti_Frame *frames, uint16_t count)
{
    uint16_t i;

    for (i = 0U; i < count; i++) {
        gVinQ15[i] = ADCCalibration_applyToInt16(
            &gVinQ15Calibration, frames[i].vin);
    }
    FIR_processBlock(&gVinFilter, gVinQ15, gFilteredVin, count);
}
```

这里使用的 `gVinFilter`、`gVinCoefficients` 和 `gVinState` 由 FIR 模块示例定义。IIR 内部可能产生大于输入的瞬态，若校准结果接近 Q15 满量程，应适当缩小 `lowValue` 与 `highValue`，为滤波中间结果保留余量。

## 示例三：四通道独立校准

每个模拟前端的零偏、增益和量程可能不同，因此四个通道应使用四份配置。

```c
static ADCCalibration_Config
    gChannelCalibration[ADC_MULTI_CHANNEL_COUNT];

typedef struct {
    int16_t vin;
    int16_t iin;
    int16_t vout;
    int16_t iout;
} CalibratedFrame;

static CalibratedFrame gCalibratedFrames[ADC_MULTI_FRAME_COUNT];

static bool Calibrations_init(void)
{
    if (!ADCCalibration_initBipolarQ15(
        &gChannelCalibration[ADC_MULTI_VIN], 82U, 2049U, 4017U)) {
        return false;
    }
    if (!ADCCalibration_initBipolarQ15(
        &gChannelCalibration[ADC_MULTI_IIN], 91U, 2054U, 4008U)) {
        return false;
    }
    if (!ADCCalibration_initBipolarQ15(
        &gChannelCalibration[ADC_MULTI_VOUT], 75U, 2045U, 4020U)) {
        return false;
    }
    return ADCCalibration_initBipolarQ15(
        &gChannelCalibration[ADC_MULTI_IOUT], 88U, 2052U, 4011U);
}

static void Calibrations_process(
    const ADCMulti_Frame *input, uint16_t count)
{
    uint16_t i;

    for (i = 0U; i < count; i++) {
        gCalibratedFrames[i].vin = ADCCalibration_applyToInt16(
            &gChannelCalibration[ADC_MULTI_VIN], input[i].vin);
        gCalibratedFrames[i].iin = ADCCalibration_applyToInt16(
            &gChannelCalibration[ADC_MULTI_IIN], input[i].iin);
        gCalibratedFrames[i].vout = ADCCalibration_applyToInt16(
            &gChannelCalibration[ADC_MULTI_VOUT], input[i].vout);
        gCalibratedFrames[i].iout = ADCCalibration_applyToInt16(
            &gChannelCalibration[ADC_MULTI_IOUT], input[i].iout);
    }
}
```

## 示例四：整块转换

ADC 四通道帧在内存中交错排列，使用块接口前应先抽取目标通道。对于已经连续存放的原始码数组，可以直接转换：

```c
static uint16_t gRawChannel[ADC_MULTI_FRAME_COUNT];
static int32_t gPhysicalValues[ADC_MULTI_FRAME_COUNT];

ADCCalibration_applyBlock(&gVinCalibration,
    gRawChannel, gPhysicalValues, ADC_MULTI_FRAME_COUNT);
```

`ADCCalibration_applyBlockToInt16()` 适合输出 Q15 或范围不超过有符号十六位的物理量。以 mV 表示高电压或以微安表示电流时可能超过该范围，应使用 `int32_t` 版本。

## 注意事项

- 标定时应使用稳定输入，并对足够多的 ADC 样本取平均。
- 零点、低端和高端应在相同参考电压、增益、量程和温度条件下测量。
- 配置只能修正零偏与分段线性增益，不能消除 ADC 或模拟前端的非线性。
- 输入参考电压、分压电阻和运放增益漂移后，应重新标定或增加温度补偿。
- 不同量程需要不同配置；切换模拟增益后必须同时切换校准参数。
- 配置对象在处理中必须保持有效，但它没有运行状态，可以被多个只读调用共享。
