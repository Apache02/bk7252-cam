#ifndef PLATFORM_UTILS_RING_BUFFER_H
#define PLATFORM_UTILS_RING_BUFFER_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Byte FIFO for one producer and one consumer.
//
// One producer and one consumer in different contexts - a task and an ISR, say - need no
// locking, because each side owns exactly one cursor and never writes the other's. Two
// producers, or two consumers, must be serialized by the caller.
//
// The cursors run free and are never wrapped; only the array index is masked. in - out
// therefore stays correct across cursor overflow, because size_t wraps at a multiple of
// the capacity - which is what forces the capacity to be a power of two.
struct ringbuf {
    uint8_t        *data; // Backing storage, mask + 1 bytes.
    size_t          mask; // Capacity - 1; the capacity is a power of two.
    volatile size_t in;   // Write cursor, owned by the producer.
    volatile size_t out;  // Read cursor, owned by the consumer.
};

// Declares a file-scope buffer together with its storage, so the two cannot disagree. A
// buffer built by hand instead must set .mask one below a power-of-two storage size.
#define RINGBUF_DECLARE(name, capacity)                                   \
    static_assert((capacity) > 0 && ((capacity) & ((capacity) - 1)) == 0, \
                  "ring buffer capacity must be a power of two");         \
    static uint8_t        name##_storage[capacity];                       \
    static struct ringbuf name = {                                        \
        .data = name##_storage,                                           \
        .mask = (capacity) - 1,                                           \
        .in   = 0,                                                        \
        .out  = 0,                                                        \
    }

// Either side may read this, but only the owner of the opposite cursor gets a stable
// answer; for the other it is a bound the peer may already have moved past.
static inline size_t ringbuf_count(const struct ringbuf *rb) { return rb->in - rb->out; }

static inline size_t ringbuf_capacity(const struct ringbuf *rb) { return rb->mask + 1; }

static inline bool ringbuf_is_empty(const struct ringbuf *rb) { return rb->in == rb->out; }

static inline bool ringbuf_is_full(const struct ringbuf *rb) { return ringbuf_count(rb) > rb->mask; }

// Appends one byte. Returns false and drops the byte when the buffer is full.
bool ringbuf_put_byte(struct ringbuf *rb, uint8_t byte);

// Removes the oldest byte into *byte. Returns false when the buffer is empty.
bool ringbuf_get_byte(struct ringbuf *rb, uint8_t *byte);

// Removes the oldest byte and return it. Returns -1 when the buffer is empty.
int ringbuf_get_char(struct ringbuf *rb);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_UTILS_RING_BUFFER_H
