# Ring Buffer 原理与使用方式

## 原理

环形缓冲区把固定内存首尾连接起来。`head` 指向下一次写入位置，`tail` 指向最旧数据。写到数组末尾后回到开头，因此不需要为了删除第一个元素而搬移后面所有数据。

它适合连接速度不同的两个部分：ADC中断不断写入，主循环或算法按自己的节奏读取。也可以持续保留最近一段波形，用于示波器触发前数据。

本模块是通用定长元素缓冲区，可以保存 `int16_t`、`int32_t`、ADC帧结构体或其他固定大小类型，不使用动态内存。

## 基本使用

```c
#include "Algorithms/data_flow/ring_buffer/ring_buffer.h"

#define BUFFER_CAPACITY  (1024U)

static int16_t gStorage[BUFFER_CAPACITY];
static RingBuffer gBuffer;

static bool SampleBuffer_init(void)
{
    return RingBuffer_init(
        &gBuffer,
        gStorage,
        BUFFER_CAPACITY,
        sizeof(gStorage[0]),
        true); /* 满时覆盖最旧数据 */
}
```

写入和读取：

```c
int16_t newSample = adcValue;
int16_t oldestSample;

RingBuffer_push(&gBuffer, &newSample);

if (RingBuffer_pop(&gBuffer, &oldestSample)) {
    /* 使用最旧数据。 */
}
```

`RingBuffer_peek()` 可以查看但不删除，适合触发后复制触发前后的波形。

## 满缓冲区策略

`overwriteWhenFull=false`：缓冲区满后拒绝新数据，适合不能丢失旧命令或记录的队列。

`overwriteWhenFull=true`：新数据覆盖最旧数据，适合示波器和连续监视，始终保留最近的N个点。

两种情况下都会增加 `overflowCount`。这个计数不能忽略：即使覆盖模式符合设计，它也能说明生产者曾经追上消费者。

## ADC帧结构体

```c
static ADCMulti_Frame gFrameStorage[256];
static RingBuffer gFrameBuffer;

RingBuffer_init(&gFrameBuffer,
    gFrameStorage, 256U,
    sizeof(ADCMulti_Frame), true);
```

通用实现内部使用 `memcpy()`，不会假设元素对齐方式。

## 并发注意事项

本模块本身不提供中断或多核并发保护。如果ADC ISR写入、主循环读取，`head/tail/count` 的组合更新可能被打断。应采用临界区、单生产者单消费者专用无锁方案，或确保相关调用处于同一执行上下文。

不要在长时间关中断的临界区内批量复制大型数据块。更好的做法是DMA双缓冲负责高速采集，环形缓冲只保存帧描述或较低速结果。

## 注意事项

- `storage` 必须在缓冲区使用期间一直有效，不能是已经返回函数的局部数组。
- 容量以元素计，存储字节数是 `capacity × elementSize`。
- `write()` 在禁止覆盖模式下可能只写入一部分，必须检查返回值和 `writtenCount`。
- `read()` 数据不足时会读出当前可用部分并返回false，必须检查 `readCount`。
- `reset()` 会立即丢弃全部未读数据并清零溢出计数。
- 环形缓冲只能缓解短时处理抖动；消费者长期慢于生产者时，任何有限容量最终都会溢出。
- 保存示波器预触发数据时通常使用覆盖模式；保存控制命令时通常禁止覆盖。

