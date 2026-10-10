#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "device/camera.h"
#include "platform/unistd.h"

// Frames are 12-27 KB depending on the scene.
#define FRAME_BUF_BYTES (48 * 1024)

static uint8_t g_frame_buf[FRAME_BUF_BYTES] __attribute__((aligned(4)));

// Full camera start, then a short check that frames really arrive.
int command_capture_start(__unused int argc, __unused const char *argv[]) {
    const camera_config_t config = {
        .sensor          = nullptr,
        .frame_buf       = g_frame_buf,
        .frame_buf_size  = sizeof(g_frame_buf),
        .frame_min_bytes = sizeof(g_frame_buf) / 2,
        .frame_max_bytes = sizeof(g_frame_buf) - 1024,
    };
    const int rc = camera_start(&config);
    if (rc != 0) {
        printf("camera start failed: %s\r\n", camera_strerror(rc));
        return 1;
    }
    printf("sensor: %s\r\n", camera_sensor_name());

    const uint32_t before = camera_frames_seen();
    usleep(500000);
    const uint32_t frames = camera_frames_seen() - before;

    printf("camera started: %lu frames in ~0.5 s, last frame %lu bytes\r\n", frames, camera_last_frame_size());
    return frames > 0 ? 0 : 1;
}

#define STREAM_SLOT_BYTES (48 * 1024)
#define STREAM_SLOT_COUNT (3)
#define STREAM_DEFAULT_S  (3)
#define STREAM_POLL_US    (1000)

// Streams for a few seconds and reports how many frames arrived whole and how many were lost.
// Usage: capture_stream [seconds]
int command_capture_stream(int argc, const char *argv[]) {
    unsigned seconds = STREAM_DEFAULT_S;
    if (argc > 1) seconds = static_cast<unsigned>(strtoul(argv[1], nullptr, 0));
    if (seconds == 0) seconds = STREAM_DEFAULT_S;

    auto *slots = static_cast<uint8_t *>(malloc(STREAM_SLOT_BYTES * STREAM_SLOT_COUNT));
    if (slots == nullptr) {
        printf("out of memory\r\n");
        return 1;
    }

    const camera_stream_config_t config = {
        .slots      = slots,
        .slot_size  = STREAM_SLOT_BYTES,
        .slot_count = STREAM_SLOT_COUNT,
        .on_frame   = nullptr,
    };
    const int rc = camera_stream_start(&config);
    if (rc != 0) {
        printf("stream start failed: %s\r\n", camera_strerror(rc));
        free(slots);
        return 1;
    }

    uint32_t frames = 0;
    uint32_t bytes  = 0;
    uint32_t biggest = 0;
    uint32_t bad_ends = 0;
    for (unsigned t = 0; t < seconds * (1000000 / STREAM_POLL_US); t++) {
        camera_frame_t frame;
        while (camera_stream_get(&frame) == 0) {
            frames++;
            bytes += frame.size;
            if (frame.size > biggest) biggest = frame.size;
            if (frame.data[0] != 0xFF || frame.data[1] != 0xD8 || frame.data[frame.size - 2] != 0xFF ||
                frame.data[frame.size - 1] != 0xD9) {
                bad_ends++;
            }
            camera_stream_release(&frame);
        }
        usleep(STREAM_POLL_US);
    }

    const uint32_t dropped = camera_stream_dropped();
    camera_stream_stop();
    free(slots);

    printf("frames %lu, dropped %lu, damaged %lu, avg %lu bytes, biggest %lu bytes, in ~%u s\r\n",
           static_cast<unsigned long>(frames), static_cast<unsigned long>(dropped),
           static_cast<unsigned long>(bad_ends), static_cast<unsigned long>(frames ? bytes / frames : 0),
           static_cast<unsigned long>(biggest), seconds);
    return frames > 0 ? 0 : 1;
}

int command_capture_frame(__unused int argc, __unused const char *argv[]) {
    camera_frame_t frame;
    const int      rc = camera_capture_frame(&frame);
    if (rc != 0) {
        printf("capture failed: %s (encoder reports a frame of %lu bytes)\r\n", camera_strerror(rc),
               camera_last_frame_size());
        return 1;
    }

    printf("addr 0x%08lx  size %lu\r\n", reinterpret_cast<unsigned long>(frame.data),
           static_cast<unsigned long>(frame.size));
    return 0;
}
