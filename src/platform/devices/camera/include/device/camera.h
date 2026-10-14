#ifndef _DEVICE_CAMERA_H
#define _DEVICE_CAMERA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *sensor;          // NULL tries every known sensor
    uint8_t    *frame_buf;       // 4-byte aligned, owned by the caller
    size_t      frame_buf_size;  // multiple of 4
    uint32_t    frame_min_bytes; // frame size bounds for the encoder's bit-rate control;
    uint32_t    frame_max_bytes; // 0 turns it off
} camera_config_t;

typedef struct {
    const uint8_t *data; // a whole JPEG, from FF D8 to FF D9
    size_t         size;
} camera_frame_t;

// All functions return 0 or a negative errno; camera_strerror() turns it into text.

// Powers the encoder and the sensor, finds the sensor, loads its init table and waits
// for it to settle. Needs CPU interrupts enabled.
int  camera_start(const camera_config_t *config);
void camera_stop(void);

// Waits for the next frame boundary, then returns the frame that follows. The data stays in
// frame_buf until the next call.
int camera_capture_frame(camera_frame_t *frame);

// Continuous capture: every frame lands in its own slot. The encoder interrupt re-arms the DMA
// on the next free slot, so no frame is lost while the caller is busy (for example sending the
// previous one over Wi-Fi). A frame is dropped only when no slot is free or it arrived damaged.
#define CAMERA_STREAM_MAX_SLOTS (8)

typedef struct {
    uint8_t *slots;      // slot_count * slot_size bytes, 4-byte aligned, owned by the caller
    size_t   slot_size;  // multiple of 4, at most 65536; frame_max_bytes + 5 or more when that is set
    unsigned slot_count; // 2 to CAMERA_STREAM_MAX_SLOTS
    void   (*on_frame)(void); // optional; runs in the interrupt after a frame is queued
} camera_stream_config_t;

// Needs a started camera. Cannot be combined with camera_capture_frame().
int  camera_stream_start(const camera_stream_config_t *config);
void camera_stream_stop(void); // slots handed out earlier become invalid

// Takes the oldest finished frame. Returns -EAGAIN when there is none. The slot stays
// the caller's until camera_stream_release().
int  camera_stream_get(camera_frame_t *frame);
void camera_stream_release(const camera_frame_t *frame);

uint32_t camera_stream_dropped(void); // frames lost since camera_stream_start()

const char *camera_sensor_name(void); // NULL when not started
uint32_t    camera_frames_seen(void);
uint32_t    camera_last_frame_size(void); // as counted by the encoder

int camera_sensor_read(uint8_t reg, uint8_t *value);
int camera_sensor_write(uint8_t reg, uint8_t value);

const char *camera_strerror(int rc);

#ifdef __cplusplus
}
#endif

#endif // _DEVICE_CAMERA_H
