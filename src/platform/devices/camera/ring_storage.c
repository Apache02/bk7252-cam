#include "ring.h"

// Kept in its own file so that an image which never starts the camera does not carry the 16 KiB: the linker
// drops whole object files that nothing references.

#define RING_BYTES (CAMERA_CHUNK_BYTES * CAMERA_CHUNK_COUNT)

static_assert((RING_BYTES & (RING_BYTES - 1)) == 0, "the ring size must be a power of two");

static uint8_t g_storage[RING_BYTES] __attribute__((aligned(4)));

struct ringbuf camera_ring = {
    .data = g_storage,
    .mask = RING_BYTES - 1,
};
