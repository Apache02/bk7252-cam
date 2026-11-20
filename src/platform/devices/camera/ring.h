#ifndef _CAMERA_RING_H
#define _CAMERA_RING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "device/camera.h"
#include "utils/ring_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

// Cursor and frame bookkeeping for the camera ring. No hardware access: the interrupts hand in plain
// numbers, the ring memory is read and written through rb->data.
//
// Only rb->in is used from struct ringbuf. It is the start of the chunk the DMA is writing now, always a
// multiple of the chunk size; everything before it is written. Cursors run free and are never wrapped.

#define RING_TRAILER_BYTES (5) // FF D9 is followed by 00 and the 32-bit frame size
#define RING_GUARD_CHUNKS  (2) // one chunk is being written, the write cursor can be one chunk late
#define RING_FRAME_SLOTS   (4) // frames remembered, indexed by id & 3

typedef enum {
    RING_FRAME_NONE,
    RING_FRAME_WRITING,
    RING_FRAME_COMPLETE,
    RING_FRAME_BAD,
} ring_frame_state_t;

typedef struct {
    uint32_t           id;
    size_t             start;   // cursor of the first byte, a multiple of the chunk size
    size_t             end;     // cursor after FF D9; valid once the frame is COMPLETE
    ring_frame_state_t state;
    bool               checked; // FF D8 seen at the start
} ring_frame_t;

typedef struct {
    struct ringbuf *rb;
    size_t          chunk;
    unsigned        chunk_shift;
    ring_frame_t    frames[RING_FRAME_SLOTS];
    uint32_t        cur_id;  // frame being written; 0 = not synced
    uint32_t        last_id; // last id handed out; survives ring_core_start() so old streams never match a new frame
    uint32_t        lost;    // finished frames that were damaged or unsynced
} ring_core_t;

// -EINVAL unless chunk is a power of two, a multiple of 4, and at most capacity / 4. Resets everything.
int ring_core_init(ring_core_t *core, struct ringbuf *rb, size_t chunk);

// first_cursor is a multiple of the chunk size: where the DMA starts writing. The first frame is unsynced.
// Frame ids keep counting from before, so streams of an earlier run read -ESTALE.
void ring_core_start(ring_core_t *core, size_t first_cursor);

// The ring of the camera driver, 16 KiB (ring_storage.c).
extern struct ringbuf camera_ring;

// DMA finished one or more chunks. past_in is the number of bytes written beyond rb->in.
// Both interrupt entry points must run with FIQ masked, and past_in must be computed under the same mask.
void ring_core_on_chunk(ring_core_t *core, size_t past_in);

// The encoder finished a frame of `size` bytes (the trailer is not counted) and the DMA has drained;
// past_in is as above. Returns the cursor to re-arm the DMA at. Runs in the FIQ.
size_t ring_core_on_frame_end(ring_core_t *core, size_t past_in, size_t size);

// 0 and an attached stream, or -EAGAIN when no frame start is in the ring.
int ring_core_open(ring_core_t *core, camera_stream_t *stream);

// > 0 bytes copied; 0 nothing ready yet; -ESTALE overwritten; -EBADMSG damaged frame; -EINVAL already at the end.
int ring_core_read(ring_core_t *core, camera_stream_t *stream, void *dst, size_t max);

#ifdef __cplusplus
}
#endif

#endif // _CAMERA_RING_H
