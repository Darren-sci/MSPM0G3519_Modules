# TIMG7 系统节拍使用说明

## SysConfig 配置

工程使用 `TIMG7` 产生固定的 1 ms 系统节拍。该定时器仅用于软件计时和周期任务调度，不需要输出引脚，也不使用 DMA。

在 SysConfig 中新增 `Timer` 实例，并设置为：

| 配置项 | 设置值 | 作用 |
|---|---|---|
| Name | `SYSTEM_TICK` | 生成 `SYSTEM_TICK_INST` 等代码宏 |
| Peripheral | `TIMG7` | 使用当前空闲的通用定时器 |
| Timer Mode | `Periodic` | 计数结束后自动重新开始 |
| Timer Period | `1 ms` | 每 1 ms 产生一次系统节拍 |
| Interrupt | `Zero` | 计数到零时进入定时器中断 |
| Start Timer | 关闭 | 由 `SystemTick_start()`在主程序中按功能开关启动 |
| Events | 关闭 | 系统节拍不触发 ADC 或其他外设 |
| DMA | 不配置 | 没有数据搬运需求 |
| Timer Output Pin | 不配置 | 软件计时不需要占用 GPIO |

对应的 `.syscfg` 配置为：

```javascript
const TIMER2 = TIMER.addInstance();

TIMER2.$name              = "SYSTEM_TICK";
TIMER2.timerMode          = "PERIODIC";
TIMER2.timerPeriod        = "1 ms";
TIMER2.interrupts         = ["ZERO"];
TIMER2.peripheral.$assign = "TIMG7";
```

## 主程序开关

系统节拍当前跟随状态机功能开关，默认关闭：

```c
#define ENABLE_STATE_MACHINE      (0)
#define STATE_MACHINE_PERIOD_MS   (10U)
```

需要使用时将开关改为 `1`：

```c
#define ENABLE_STATE_MACHINE      (1)
```

主程序在进入循环前启动一次定时器：

```c
SystemTick_start();
```

在主循环中通过周期判断运行状态机：

```c
if (SystemTick_isDue(
        &gStateMachineLastTimeMs, STATE_MACHINE_PERIOD_MS)) {
    /* 在这里编写需要按固定周期反复执行的状态机代码。 */
}
```

每一个周期任务都必须使用独立的上次执行时间变量，不能让多个任务共用同一个变量。

## 中断执行原则

`TIMG7` 中断只负责将毫秒计数加一。LCD 刷新、算法计算、通信和业务状态机均放在主循环的周期判断中执行，避免长时间占用中断。
