#include "utils/ring_buffer.h"


// A cursor may only be published once the byte it accounts for has landed, or the peer
// acts on a slot nobody has written. No hardware barrier is needed: this core is in-order
// and an ISR observes every store the interrupted code had already made. The compiler is
// the open question - volatile orders the cursors against each other, but the standard
// still lets a plain store into data[] sink past a volatile one.
#define RINGBUF_COMPILER_BARRIER() __asm__ volatile("" ::: "memory")


bool ringbuf_put_byte(struct ringbuf *rb, uint8_t byte) {
    if (ringbuf_is_full(rb)) return false;

    // Ours alone, so it cannot move under us: read once, publish in a single store.
    const size_t in = rb->in;

    rb->data[in & rb->mask] = byte;
    RINGBUF_COMPILER_BARRIER();
    rb->in = in + 1;

    return true;
}

bool ringbuf_get_byte(struct ringbuf *rb, uint8_t *byte) {
    if (ringbuf_is_empty(rb)) return false;

    // Likewise ours alone.
    const size_t out = rb->out;

    *byte = rb->data[out & rb->mask];
    RINGBUF_COMPILER_BARRIER();
    rb->out = out + 1;

    return true;
}

int ringbuf_get_char(struct ringbuf *rb) {
    uint8_t byte;
    if (!ringbuf_get_byte(rb, &byte)) return -1;

    return byte;
}
