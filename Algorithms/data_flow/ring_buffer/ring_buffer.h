#ifndef ALGORITHMS_DATA_FLOW_RING_BUFFER_H_
#define ALGORITHMS_DATA_FLOW_RING_BUFFER_H_

#include <stdbool.h>
#include <stdint.h>

/** 调用者提供存储区的通用定长元素环形缓冲区。 */
typedef struct {
    uint8_t *storage;
    uint32_t capacity;
    uint32_t elementSize;
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint64_t overflowCount;
    bool overwriteWhenFull;
    bool initialized;
} RingBuffer;

/**
 * 初始化环形缓冲区。storage 至少包含 capacity*elementSize 字节。
 * overwriteWhenFull=true 时，新数据会覆盖最旧数据；否则满时拒绝写入。
 */
bool RingBuffer_init(RingBuffer *buffer, void *storage,
    uint32_t capacity, uint32_t elementSize,
    bool overwriteWhenFull);

/** 清空数据和溢出计数，但保留配置。 */
bool RingBuffer_reset(RingBuffer *buffer);

/** 写入一个元素。 */
bool RingBuffer_push(RingBuffer *buffer, const void *element);

/** 弹出最旧元素。 */
bool RingBuffer_pop(RingBuffer *buffer, void *element);

/** 查看距最旧元素 offset 个位置的数据，但不移除。 */
bool RingBuffer_peek(const RingBuffer *buffer,
    uint32_t offset, void *element);

/** 批量写入，writtenCount 返回实际接收数量。 */
bool RingBuffer_write(RingBuffer *buffer, const void *elements,
    uint32_t elementCount, uint32_t *writtenCount);

/** 批量读取并移除，readCount 返回实际数量。 */
bool RingBuffer_read(RingBuffer *buffer, void *elements,
    uint32_t elementCount, uint32_t *readCount);

/** 丢弃最旧的 elementCount 个元素；数量超过现有数据时返回 false。 */
bool RingBuffer_discard(RingBuffer *buffer, uint32_t elementCount);

uint32_t RingBuffer_getCount(const RingBuffer *buffer);
uint32_t RingBuffer_getFreeCount(const RingBuffer *buffer);
uint64_t RingBuffer_getOverflowCount(const RingBuffer *buffer);
bool RingBuffer_isEmpty(const RingBuffer *buffer);
bool RingBuffer_isFull(const RingBuffer *buffer);

#endif
