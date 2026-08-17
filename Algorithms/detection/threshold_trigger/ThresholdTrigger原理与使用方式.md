# Threshold Trigger 原理与使用方式

## 原理

阈值触发检测波形穿越指定电平的时刻：从阈值下方到上方是上升沿，从上方到下方是下降沿。它是数字示波器稳定显示、周期同步和事件捕获的基础。

简单比较 `sample >= threshold` 会在噪声附近反复触发，因此模块使用迟滞重新武装。以上升沿为例，信号必须先下降到 `threshold-hysteresis` 以下才允许下一次上升触发；下降沿则必须先上升到 `threshold+hysteresis` 以上。

触发后还可以设置 holdoff，在指定采样数内忽略后续穿越，避免一个复杂脉冲或振铃产生多个触发。

## 亚采样插值

真实阈值穿越通常发生在两个ADC采样之间。模块根据前后两个采样进行线性插值，并用 Q16.16采样位置返回：

```text
sampleIndexQ16 = 98304
98304 / 65536 = 1.5个采样位置
```

插值能改善周期和相位显示，但无法恢复采样率以上的信息；边沿过快、噪声很大时仍受ADC采样限制。

## 使用方式

```c
static ThresholdTrigger_State gTrigger;

static const ThresholdTrigger_Config gTriggerConfig = {
    .threshold = 0,
    .hysteresis = 200U,
    .holdoffSamples = 100U,
    .edge = THRESHOLD_TRIGGER_RISING
};

static bool Trigger_init(void)
{
    return ThresholdTrigger_init(
        &gTrigger, &gTriggerConfig);
}
```

处理一个DMA数据块并收集事件：

```c
ThresholdTrigger_Event events[4];
uint32_t eventCount;
bool eventOverflow;

ThresholdTrigger_processBlock(
    &gTrigger,
    samples,
    sampleCount,
    events,
    4U,
    &eventCount,
    &eventOverflow);
```

事件位置是从最近一次 `reset()` 开始的全局采样位置，不是当前DMA块内下标。因此跨数据块的阈值穿越仍能正确插值。

## 与环形缓冲区组成示波器

```text
ADC连续采样
→ Ring Buffer持续覆盖，保存最近的预触发数据
→ Threshold Trigger发现边沿
→ 记录触发位置
→ 再采集指定数量的触发后数据
→ 冻结并复制完整波形
```

例如希望触发点位于屏幕30%位置，就保留30%的触发前环形数据，再采集70%的触发后数据。

## 注意事项

- 阈值、迟滞和输入必须使用同一种数据单位。
- 迟滞过小会受噪声反复触发，过大可能漏掉小幅信号。
- holdoff过短可能捕获振铃，过长则可能漏掉下一真实周期。
- `THRESHOLD_TRIGGER_EITHER` 会报告上下两个方向，事件中带有实际边沿类型。
- 事件数组容量不足时模块仍继续更新状态，并设置 `eventOverflow=true`。
- 切换通道、阈值、量程或开始新捕获时，应重新初始化或reset。
- 触发只负责确定时间位置，不负责停止DMA、冻结环形缓冲或复制显示波形，这些属于示波器应用层状态机。
