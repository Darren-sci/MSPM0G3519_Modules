#include "Algorithms/data_flow/ring_buffer/ring_buffer.h"

#include <stddef.h>
#include <string.h>

static bool RingBuffer_isValid(const RingBuffer *buffer)
{
    return (buffer != 0) && buffer->initialized &&
           (buffer->storage != 0) && (buffer->capacity != 0U) &&
           (buffer->elementSize != 0U) &&
           (buffer->head < buffer->capacity) &&
           (buffer->tail < buffer->capacity) &&
           (buffer->count <= buffer->capacity);
}

static uint8_t *RingBuffer_elementAddress(
    const RingBuffer *buffer, uint32_t index)
{
    return buffer->storage + (size_t) index * buffer->elementSize;
}

static uint32_t RingBuffer_nextIndex(
    const RingBuffer *buffer, uint32_t index)
{
    index++;
    return (index == buffer->capacity) ? 0U : index;
}

bool RingBuffer_init(RingBuffer *buffer, void *storage,
    uint32_t capacity, uint32_t elementSize,
    bool overwriteWhenFull)
{
    if ((buffer == 0) || (storage == 0) ||
        (capacity == 0U) || (elementSize == 0U)) {
        return false;
    }
#if SIZE_MAX <= UINT32_MAX
    if (capacity > (uint32_t) (SIZE_MAX / elementSize)) {
        return false;
    }
#endif

    buffer->storage = (uint8_t *) storage;
    buffer->capacity = capacity;
    buffer->elementSize = elementSize;
    buffer->overwriteWhenFull = overwriteWhenFull;
    buffer->initialized = true;
    return RingBuffer_reset(buffer);
}

bool RingBuffer_reset(RingBuffer *buffer)
{
    if ((buffer == 0) || !buffer->initialized) {
        return false;
    }
    buffer->head = 0U;
    buffer->tail = 0U;
    buffer->count = 0U;
    buffer->overflowCount = 0U;
    return true;
}

bool RingBuffer_push(RingBuffer *buffer, const void *element)
{
    if (!RingBuffer_isValid(buffer) || (element == 0)) {
        return false;
    }

    if (buffer->count == buffer->capacity) {
        if (buffer->overflowCount != UINT64_MAX) {
            buffer->overflowCount++;
        }
        if (!buffer->overwriteWhenFull) {
            return false;
        }
        buffer->tail = RingBuffer_nextIndex(buffer, buffer->tail);
    } else {
        buffer->count++;
    }

    memcpy(RingBuffer_elementAddress(buffer, buffer->head),
        element, buffer->elementSize);
    buffer->head = RingBuffer_nextIndex(buffer, buffer->head);
    return true;
}

bool RingBuffer_pop(RingBuffer *buffer, void *element)
{
    if (!RingBuffer_isValid(buffer) || (element == 0) ||
        (buffer->count == 0U)) {
        return false;
    }

    memcpy(element, RingBuffer_elementAddress(buffer, buffer->tail),
        buffer->elementSize);
    buffer->tail = RingBuffer_nextIndex(buffer, buffer->tail);
    buffer->count--;
    return true;
}

bool RingBuffer_peek(const RingBuffer *buffer,
    uint32_t offset, void *element)
{
    uint32_t index;

    if (!RingBuffer_isValid(buffer) || (element == 0) ||
        (offset >= buffer->count)) {
        return false;
    }

    index = (offset >= buffer->capacity - buffer->tail) ?
        offset - (buffer->capacity - buffer->tail) :
        buffer->tail + offset;
    memcpy(element, RingBuffer_elementAddress(buffer, index),
        buffer->elementSize);
    return true;
}

bool RingBuffer_write(RingBuffer *buffer, const void *elements,
    uint32_t elementCount, uint32_t *writtenCount)
{
    const uint8_t *source = (const uint8_t *) elements;
    uint32_t index;

    if (writtenCount == 0) {
        return false;
    }
    *writtenCount = 0U;
    if (!RingBuffer_isValid(buffer)) {
        return false;
    }
    if (elementCount == 0U) {
        return true;
    }
    if (elements == 0) {
        return false;
    }
#if SIZE_MAX <= UINT32_MAX
    if (elementCount > (uint32_t) (SIZE_MAX / buffer->elementSize)) {
        return false;
    }
#endif

    for (index = 0U; index < elementCount; index++) {
        if (!RingBuffer_push(buffer,
                source + (size_t) index * buffer->elementSize)) {
            return false;
        }
        (*writtenCount)++;
    }
    return true;
}

bool RingBuffer_read(RingBuffer *buffer, void *elements,
    uint32_t elementCount, uint32_t *readCount)
{
    uint8_t *destination = (uint8_t *) elements;

    if (readCount == 0) {
        return false;
    }
    *readCount = 0U;
    if (!RingBuffer_isValid(buffer)) {
        return false;
    }
    if (elementCount == 0U) {
        return true;
    }
    if (elements == 0) {
        return false;
    }
#if SIZE_MAX <= UINT32_MAX
    if (elementCount > (uint32_t) (SIZE_MAX / buffer->elementSize)) {
        return false;
    }
#endif

    while ((*readCount < elementCount) && (buffer->count != 0U)) {
        if (!RingBuffer_pop(buffer,
                destination + (size_t) *readCount *
                    buffer->elementSize)) {
            return false;
        }
        (*readCount)++;
    }
    return *readCount == elementCount;
}

bool RingBuffer_discard(RingBuffer *buffer, uint32_t elementCount)
{
    uint32_t advance;

    if (!RingBuffer_isValid(buffer) ||
        (elementCount > buffer->count)) {
        return false;
    }
    if (elementCount == 0U) {
        return true;
    }

    advance = elementCount % buffer->capacity;
    buffer->tail = (advance >= buffer->capacity - buffer->tail) ?
        advance - (buffer->capacity - buffer->tail) :
        buffer->tail + advance;
    buffer->count -= elementCount;
    return true;
}

uint32_t RingBuffer_getCount(const RingBuffer *buffer)
{
    return RingBuffer_isValid(buffer) ? buffer->count : 0U;
}

uint32_t RingBuffer_getFreeCount(const RingBuffer *buffer)
{
    return RingBuffer_isValid(buffer) ?
        buffer->capacity - buffer->count : 0U;
}

uint64_t RingBuffer_getOverflowCount(const RingBuffer *buffer)
{
    return RingBuffer_isValid(buffer) ? buffer->overflowCount : 0U;
}

bool RingBuffer_isEmpty(const RingBuffer *buffer)
{
    return RingBuffer_isValid(buffer) && (buffer->count == 0U);
}

bool RingBuffer_isFull(const RingBuffer *buffer)
{
    return RingBuffer_isValid(buffer) &&
           (buffer->count == buffer->capacity);
}
