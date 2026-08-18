# PID 控制器原理与使用方式

## 1. 用途与结构

`pid.c/.h` 是通用离散 PID 控制器，可用于稳压、稳流、功率、温度、DAC 幅值、转速等闭环。它在 PI 的基础上增加微分项：比例响应当前误差，积分消除稳态误差，微分根据变化趋势提前制动。

每次调用 `PID_update()` 的处理顺序为：误差与死区 → P → I → D → 前馈 → 输出限幅、抗积分饱和和斜率限制。

## 2. 关键实现

- `kpQ16`、`kiQ16PerSecond`、`kdQ16Second` 均为 Q16.16。积分增益按采样周期换算，微分增益按采样周期换算为差分系数。
- `derivativeOnMeasurement=true` 时，D 项对测量值求差分而不是对设定值求差分，可避免设定值阶跃造成微分冲击，通常更适合模拟量闭环。
- `derivativeFilterQ15` 是微分低通系数：0 表示不滤波，32768 接近完全跟随；噪声较大时选中间值。
- PI 中的输出限幅、积分限幅、条件积分、反算抗饱和、前馈、死区、斜率限制和手动/自动无扰切换在 PID 中全部保留。

## 3. 接口调用示例

```c
#include "Algorithms/control/pid/pid.h"

static PID_Controller loop;
static const PID_Config cfg = {
    .kpQ16 = 19661, .kiQ16PerSecond = 2621, .kdQ16Second = 655,
    .samplePeriodUs = 1000,
    .outputMin = 0, .outputMax = 4095,
    .integratorMin = 0, .integratorMax = 4095,
    .deadband = 1, .feedForward = 0,
    .outputRiseLimit = 80, .outputFallLimit = 120,
    .backCalculationGainQ16 = 16384,
    .derivativeFilterQ15 = 8192,
    .derivativeOnMeasurement = true,
    .action = PI_ACTION_DIRECT,
    .antiWindup = PI_ANTI_WINDUP_CONDITIONAL
};

void loop_init(void) {
    (void)PID_init(&loop, &cfg);
    (void)PID_reset(&loop, 2000);
}

void loop_tick(int32_t reference, int32_t feedback) {
    PID_Result result;
    if (PID_update(&loop, reference, feedback, &result)) {
        DAC_writeCode((uint16_t)result.output);
    }
}
```

若反馈噪声很大，先使用 PI；必须使用 PID 时，优先采用测量值微分并降低 `kd`、设置微分滤波。设定值、反馈值和输出必须使用可比较且不溢出的工程单位。

## 4. 注意事项

1. 定时器周期必须固定，并与 `samplePeriodUs` 一致。不要在不规则循环中直接调用。
2. D 项对噪声非常敏感；ADC 原始码通常需要校准和适度低通，不能用极大的 `kd` 弥补传感器噪声。
3. 先关闭 D 项调好 PI，再逐步增加 D 项。出现高频抖动时先减小 `kd` 或加大滤波。
4. 输出饱和时观察 `outputLimited` 与 `integratorLimited`，检查执行器是否已达到物理极限。
5. 手动调试时调用 `PID_setManualMode()`，恢复闭环前调用 `PID_setAutomaticMode()` 完成无扰接管。
