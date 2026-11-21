#ifndef _DEVICE_CAMERA_H
#define _DEVICE_CAMERA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The camera runs one capture path: the JPEG encoder feeds a ring of fixed-size chunks that is smaller than a
// frame. Nothing is kept per frame. A frame is read while it is being written: a reader finds the start of a
// frame in the ring, copies the chunks that are ready, and gets an error if the encoder overwrote them first.
//
// All functions return 0 or a negative errno unless stated otherwise; camera_strerror() turns it into text.
// Reading functions can run in any number of tasks at once. camera_start() and camera_stop() must not.
// Needs CPU IRQ and FIQ enabled: the end of a frame arrives as a FIQ.

#define CAMERA_WAIT_FOREVER (UINT32_MAX)

// The ring is fixed and lives inside the driver: 8 chunks of 2048 bytes, 16 KiB. A frame is bigger than that.
// A reader may be at most CAMERA_READER_LAG bytes behind the encoder (the ring minus two free chunks) before
// the data it wants is overwritten.
#define CAMERA_CHUNK_BYTES (2048)
#define CAMERA_CHUNK_COUNT (8)
#define CAMERA_READER_LAG  ((CAMERA_CHUNK_COUNT - 2) * CAMERA_CHUNK_BYTES)

typedef struct {
    const char *sensor; // NULL tries every known sensor

    // Bounds for the encoder's bit-rate control, in bytes per frame. 0 turns it off.
    uint32_t frame_min_bytes;
    uint32_t frame_max_bytes;
} camera_config_t;

typedef struct {
    const char *sensor; // name of the sensor that answered
    uint16_t    width;
    uint16_t    height;
} camera_info_t;

// Powers the encoder and the sensor, finds the sensor, loads its init table, waits for it to settle and starts
// the capture. After it returns 0 frames are being written into the ring.
// Calling it on a running camera returns 0 and ignores `config`; it returns -EBUSY while another task is still
// starting it. -ENODEV: no sensor answered. -EIO: a sensor register write failed. -ETIMEDOUT: no frames arrived.
int  camera_start(const camera_config_t *config);
void camera_stop(void); // invalidates every open stream

// Returns -ENXIO when the camera is not started.
int camera_get_info(camera_info_t *info);

// One whole frame (FF D8 ... FF D9) into the caller's buffer: camera_stream_open() plus camera_stream_read()
// until the frame ends. Returns the frame size (> 0). A frame that was damaged or overwritten before it was
// copied is skipped and the next one is tried until the timeout. -ETIMEDOUT: no good frame in time (0 = do not wait).
// -ENOSPC: the frame is larger than dst_size. -ENXIO: not started.
int camera_read_frame(void *dst, size_t dst_size, uint32_t timeout_ms);

// Streaming: follow one frame while it is written and send its chunks as they get ready. A stream is a small
// cursor owned by the caller; streams do not affect each other or the encoder.
typedef struct {
    uint32_t frame_id; // frame being followed; frame_id + 1 is the next one
    size_t   position; // private
    bool     at_end;   // private, see camera_stream_at_end()
} camera_stream_t;

// Waits until the start of a frame is in the ring and attaches to the newest such frame. A frame whose start
// the encoder has already overwritten is not offered.
// -ETIMEDOUT: no frame start in time (0 = do not wait). -ENXIO: not started.
int camera_stream_open(camera_stream_t *stream, uint32_t timeout_ms);

// Copies the next bytes of the frame into dst, at most `max`, whatever is ready now; waits for at least one
// byte. Returns the number of bytes copied (> 0), the trailer after FF D9 is never included.
// -ETIMEDOUT: nothing new in time (0 = do not wait); the stream stays open.
// -ESTALE: the encoder overwrote the data before it was copied: this frame is lost.
// -EBADMSG: the frame ended damaged: it is lost. After -ESTALE or -EBADMSG open a new stream; bytes of the
// lost frame that were already sent are a broken frame for the receiver.
// -ENXIO: not started or stopped meanwhile.
int  camera_stream_read(camera_stream_t *stream, void *dst, size_t max, uint32_t timeout_ms);
bool camera_stream_at_end(const camera_stream_t *stream); // true once the last bytes of the frame were returned

typedef struct {
    uint32_t frames;           // frames the encoder finished since camera_start()
    uint32_t frames_lost;      // of those, frames that were damaged or arrived unsynced and cannot be read
    uint32_t last_frame_bytes; // size of the last finished frame as counted by the encoder; 0 before the first
} camera_stats_t;

void camera_get_stats(camera_stats_t *stats); // zeros when not started

// Raw access to the sensor's 8-bit registers, for debugging.
int camera_sensor_read(uint8_t reg, uint8_t *value);
int camera_sensor_write(uint8_t reg, uint8_t value);

const char *camera_strerror(int rc);

#ifdef __cplusplus
}
#endif

#endif // _DEVICE_CAMERA_H
