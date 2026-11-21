#include "ring.h"
#include <errno.h>
#include <string.h>
#include "platform/cpu.h"

static inline size_t ring_capacity(const ring_core_t *core) { return core->rb->mask + 1; }

// Bytes behind rb->in that a reader may still rely on.
static inline size_t safe_lag(const ring_core_t *core) {
    return ring_capacity(core) - (RING_GUARD_CHUNKS << core->chunk_shift);
}

static inline size_t align_up(const ring_core_t *core, const size_t cursor) {
    return (cursor + core->chunk - 1) & ~(core->chunk - 1);
}

static inline uint8_t byte_at(const ring_core_t *core, const size_t cursor) {
    return core->rb->data[cursor & core->rb->mask];
}

static inline ring_frame_t *slot(ring_core_t *core, const uint32_t id) { return &core->frames[id % RING_FRAME_SLOTS]; }

int ring_core_init(ring_core_t *core, struct ringbuf *rb, const size_t chunk) {
    const size_t capacity = rb->mask + 1;
    if (chunk < 4 || (chunk & (chunk - 1)) != 0 || (chunk & 3) != 0 || chunk > capacity / 4) return -EINVAL;

    memset(core, 0, sizeof(*core));
    core->rb    = rb;
    core->chunk = chunk;
    while (((size_t)1 << core->chunk_shift) < chunk) core->chunk_shift++;
    return 0;
}

void ring_core_start(ring_core_t *core, const size_t first_cursor) {
    memset(core->frames, 0, sizeof(core->frames));
    core->cur_id = 0;
    core->lost   = 0;
    core->rb->in = first_cursor;
}

void ring_core_on_chunk(ring_core_t *core, const size_t past_in) {
    const size_t chunks = past_in >> core->chunk_shift;
    if (chunks == 0) return;

    const size_t new_in = core->rb->in + (chunks << core->chunk_shift);

    if (core->cur_id != 0) {
        ring_frame_t *frame = slot(core, core->cur_id);
        if (frame->state == RING_FRAME_WRITING && !frame->checked && new_in - frame->start >= core->chunk) {
            if (byte_at(core, frame->start) == 0xFF && byte_at(core, frame->start + 1) == 0xD8) {
                frame->checked = true;
            } else {
                frame->state = RING_FRAME_BAD;
                core->cur_id = 0;
            }
        }
    }

    core->rb->in = new_in;
}

static void begin_frame(ring_core_t *core, const size_t start) {
    core->last_id++;
    if (core->last_id == 0) core->last_id = 1;
    core->cur_id = core->last_id;

    ring_frame_t *frame = slot(core, core->cur_id);
    frame->id           = core->cur_id;
    frame->start        = start;
    frame->end          = start;
    frame->state        = RING_FRAME_WRITING;
    frame->checked      = false;
}

size_t ring_core_on_frame_end(ring_core_t *core, const size_t past_in, const size_t size) {
    const size_t written_end = core->rb->in + past_in;
    const size_t next        = align_up(core, written_end);

    if (core->cur_id == 0) {
        core->lost++;
    } else {
        ring_frame_t *frame = slot(core, core->cur_id);
        const size_t  start = frame->start;

        // A frame that did not pass the first chunk is still whole in the ring, so its start can be checked here.
        bool good = frame->state == RING_FRAME_WRITING && size >= 4 && written_end - start == size + RING_TRAILER_BYTES &&
                    byte_at(core, start + size - 2) == 0xFF && byte_at(core, start + size - 1) == 0xD9;
        if (good && !frame->checked) good = byte_at(core, start) == 0xFF && byte_at(core, start + 1) == 0xD8;

        frame->end   = start + size;
        frame->state = good ? RING_FRAME_COMPLETE : RING_FRAME_BAD;
        if (!good) core->lost++;
    }

    core->rb->in = next;
    begin_frame(core, next);
    return next;
}

int ring_core_open(ring_core_t *core, camera_stream_t *stream) {
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();

    int rc = -EAGAIN;
    if (core->cur_id != 0) {
        const ring_frame_t *frame = slot(core, core->cur_id);
        if (core->rb->in - frame->start <= safe_lag(core)) {
            stream->frame_id = frame->id;
            stream->position = frame->start;
            stream->at_end   = false;
            rc               = 0;
        }
    }

    GLOBAL_INT_RESTORE();
    return rc;
}

int ring_core_read(ring_core_t *core, camera_stream_t *stream, void *dst, const size_t max) {
    if (stream->at_end) return -EINVAL;

    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    const ring_frame_t frame = *slot(core, stream->frame_id);
    const size_t       in    = core->rb->in;
    GLOBAL_INT_RESTORE();

    const size_t pos = stream->position;
    if (frame.id != stream->frame_id) return -ESTALE;
    if (frame.state == RING_FRAME_BAD) return -EBADMSG;
    if (in - pos > safe_lag(core)) return -ESTALE;

    const size_t limit = frame.state == RING_FRAME_WRITING ? in : frame.end;
    size_t       n     = limit - pos;
    if (n > max) n = max;
    if (n == 0) return 0;

    const size_t index = pos & core->rb->mask;
    size_t       first = ring_capacity(core) - index;
    if (first > n) first = n;
    memcpy(dst, core->rb->data + index, first);
    if (n > first) memcpy((uint8_t *)dst + first, core->rb->data, n - first);

    if (core->rb->in - pos > safe_lag(core)) return -ESTALE;

    stream->position = pos + n;
    stream->at_end   = frame.state == RING_FRAME_COMPLETE && stream->position == frame.end;
    return (int)n;
}
