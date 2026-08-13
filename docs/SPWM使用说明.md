# SPWM 双通道模块使用说明

## 功能

本模块使用 MSPM0G3519 的一个 PWM 定时器产生单路或双路 SPWM：

- 通道 0：`TIMA0_CCP0`，输出引脚 `PA0`
- 通道 1：`TIMA0_CCP1`，输出引脚 `PA1`
- PWM 载波和占空比更新频率：20 kHz
- 两路的正弦频率、幅度和相位可以分别设置
- 只启动一路就是单路 SPWM，同时启动两路就是双路 SPWM

引脚直接输出的是高频脉冲，需要经过 RC 或 LC 低通滤波器才能得到平滑的正弦波。滤波后的信号带有约为电源一半的直流偏置；需要以 0 V 为中心时，还要增加隔直电容或运放调理。

## 快速使用

```c
#include "Drivers/spwm.h"

int main(void)
{
    SYSCFG_DL_init();
    SPWM_init();

    /* PA0：1 kHz、80%幅度、0°。 */
    SPWM_set(SPWM_CHANNEL_0, 1000U, 800U, 0U);
    SPWM_start(SPWM_CHANNEL_0);

    while (1) {
        __WFI();
    }
}
```

双路正交信号示例：

```c
SPWM_set(SPWM_CHANNEL_0, 1000U, 800U, 0U);
SPWM_set(SPWM_CHANNEL_1, 1000U, 800U, 90U);
SPWM_start(SPWM_CHANNEL_0);
SPWM_start(SPWM_CHANNEL_1);
```

运行时可分别调整参数：

```c
SPWM_setFrequency(SPWM_CHANNEL_0, 500U);
SPWM_setAmplitude(SPWM_CHANNEL_0, 600U);
SPWM_setPhase(SPWM_CHANNEL_1, 180U);
SPWM_stop(SPWM_CHANNEL_1);
SPWM_stopAll();
```

## 参数含义

- `channel`：只能使用 `SPWM_CHANNEL_0` 或 `SPWM_CHANNEL_1`。
- `frequencyHz`：滤波后正弦波频率，单位 Hz。当前接口使用整数 Hz。
- `amplitudePermille`：幅度千分比，范围 0～1000。1000 表示最大调制度，不等于滤波后固定的某个电压值；实际电压还受 IO 电压、滤波器和后级增益影响。
- `phaseDegree`：起始相位，单位为度；360°与0°相同。

推荐把输出正弦频率控制在数百 Hz 到约 2 kHz。虽然软件允许更高频率，但频率越接近20 kHz载波，单个正弦周期包含的更新点越少，滤波后的波形越差。

## SysConfig 必需设置

打开 `empty.syscfg`，PWM 实例 `PWM_0` 应保持以下设置：

| 设置项 | 当前值 | 作用 |
|---|---:|---|
| Timer | `TIMA0` | 提供双路 PWM |
| Timer Clock | `BUSCLK / 1` | 当前为 32 MHz |
| PWM Mode | `Center Align` | 产生中心对齐 PWM |
| PWM Period Count | `1600` | 得到 20 kHz 载波 |
| Channels | `CCP0`、`CCP1` | 两路 SPWM |
| Pins | `PA0`、`PA1` | 实际输出引脚 |
| Channel Update Mode | `Zero Event` | 比较值只在周期边界生效，减少毛刺 |
| Interrupt | `Zero Event` | 每个载波周期更新正弦占空比 |
| Start Timer | 关闭 | 由 `SPWM_start()` 启动 |

中心对齐模式下，SysConfig 已按硬件计数方式计算实际装载值。不要根据“向上再向下”自行把 `1600` 改成 `800`，应以 SysConfig 显示的 `Calculated PWM Frequency = 20 kHz` 为准。

## 使用注意事项

1. 必须先调用 `SYSCFG_DL_init()`，再调用 `SPWM_init()`。
2. 不要在其他文件中再定义 `TIMA0_IRQHandler()`，否则会发生中断函数重名。
3. SPWM运行时，不要同时用原来的 `PWMOutput_setChannel0Duty()` 或 `PWMOutput_setChannel1Duty()` 修改 TIMA0 比较值。
4. 两路共用同一个20 kHz载波，但滤波后的正弦频率、幅度和相位可以不同。
5. 这两路是普通信号输出，不带互补、死区和硬件故障关断，不能直接连接半桥上下管。
6. `SPWM_stop()` 将该路恢复为50%占空比；滤波后仍有中点直流电压，但交流分量为0。为让比较值在周期边界无毛刺地生效，启动过一次后 TIMA0 会继续运行。

## 外接滤波器

最简单的测试可以从两级 RC 低通开始。例如生成1 kHz正弦时，滤波器截止频率可先选在2～3 kHz附近，再根据幅度衰减和20 kHz纹波实际调整。若要带负载，应在滤波后增加运放缓冲；单片机引脚不能直接驱动低阻或大功率负载。
