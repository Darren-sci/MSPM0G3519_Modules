# PI 控制器原理与使用方式

## 1. 用途

`pi.c/.h` 是通用离散 PI 闭环控制器。设定值和测量值使用同一工程单位，输出也是执行器单位，例如 DAC 码、电压码、PWM 码、电流设定值或功率指令。

控制流程是：误差计算 → 比例项 → 积分项 → 前馈相加 → 输出限幅与斜率限制 → 执行器。比例负责快速响应，积分负责消除稳定误差。

## 2. 关键实现

- `PI_update()` 是每个采样周期调用一次的核心函数。误差由设定值减测量值获得，`PI_ACTION_REVERSE` 可反转方向。
- 增益采用 Q16.16：整数 `65536` 表示 1.0。`kiQ16PerSecond` 是每秒积分增益，函数按 `samplePeriodUs` 自动换算为每次更新的增量。
- 积分量保存在 Q16.16 中，再转换为输出整数；`integratorMin/Max` 防止积分本身溢出。
- 输出先进行限幅，再按 `outputRiseLimit/outputFallLimit` 做每周期变化限制。抗积分饱和可选关闭、条件积分或反算。
- `PI_setManualMode()` 和 `PI_setAutomaticMode()` 支持手动/自动切换；自动接管时会按当前输出反算积分量，减少跳变。

## 3. 接口调用示例

```c
#include "Algorithms/control/pi/pi.h"
#include "Drivers/dac_output.h"

static PI_Controller voltageLoop;
static const PI_Config cfg = {
    .kpQ16 = 9830,                 /* 0.15 */
    .kiQ16PerSecond = 3277,        /* 0.05/s */
    .samplePeriodUs = 1000,        /* 1 kHz */
    .outputMin = 0, .outputMax = 4095,
    .integratorMin = 0, .integratorMax = 4095,
    .deadband = 2, .feedForward = 0,
    .outputRiseLimit = 100, .outputFallLimit = 150,
    .backCalculationGainQ16 = 32768,
    .action = PI_ACTION_DIRECT,
    .antiWindup = PI_ANTI_WINDUP_CONDITIONAL
};

void control_init(void) {
    (void)PI_init(&voltageLoop, &cfg);
    (void)PI_reset(&voltageLoop, 2048);
}

void control_tick(int32_t reference, int32_t adcValue) {
    PI_Result info;
    if (PI_update(&voltageLoop, reference, adcValue, &info)) {
        (void)DACOutput_setCode((uint16_t)info.output);
        /* info.outputLimited 可用于故障/饱和指示 */
    }
}
```

运行中可用 `PI_setTunings()`、`PI_setOutputLimits()`、`PI_setFeedForward()` 等接口在线调整参数。若 ADC 量程是毫伏而 DAC 是码值，应先统一单位或明确增益的单位。

## 4. 注意事项

1. `samplePeriodUs` 必须与真实定时中断一致；周期变化后调用 `PI_setSamplePeriod()`。
2. 输出限幅应覆盖执行器安全范围，积分限幅应覆盖合理的积分贡献，不能只依赖整数溢出保护。
3. ADC 噪声较大时先做校准、去直流或适度低通；死区只用于抑制小误差抖动，不应代替滤波。
4. 反向执行器（误差增大时输出应减小）使用 `PI_ACTION_REVERSE`，不要把负号同时写进所有增益。
5. 先用较小的 `ki` 调到基本稳定，再逐步增加；出现持续振荡时降低 `kp/ki` 或加大采样频率。
