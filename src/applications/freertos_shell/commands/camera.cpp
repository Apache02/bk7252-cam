#include <FreeRTOS.h>
#include <task.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "camera_session.h"
#include "device/camera.h"
#include "board.h"

#ifdef CAMERA_I2C

// Frames are 12-27 KB depending on the scene.
#define FRAME_BUF_BYTES   (48 * 1024)
#define STREAM_DEFAULT_S  (3)
#define STREAM_CHUNK_BYTES (1024)
#define STREAM_WAIT_MS    (1000)
#define FRAME_WAIT_MS     (2000)

// Follows frames with the streaming API for a few seconds and reports how many arrived whole, how many the camera
// overwrote before they were read, and how many were damaged. The reader does nothing slow, so the numbers show
// the best case.
// Usage: capture_stream [seconds]
int command_capture_stream(int argc, const char *argv[]) {
    unsigned seconds = STREAM_DEFAULT_S;
    if (argc > 1) seconds = static_cast<unsigned>(strtoul(argv[1], nullptr, 0));
    if (seconds == 0) seconds = STREAM_DEFAULT_S;

    auto *chunk = static_cast<uint8_t *>(malloc(STREAM_CHUNK_BYTES));
    if (chunk == nullptr) {
        printf("out of memory\r\n");
        return 1;
    }

    CameraSession session;
    const int     start_rc = session.ensure_started();
    if (start_rc != 0) {
        printf("camera start failed: %s\r\n", camera_strerror(start_rc));
        free(chunk);
        return 1;
    }

    uint32_t frames  = 0;
    uint32_t bytes   = 0;
    uint32_t biggest = 0;
    uint32_t damaged = 0;
    uint32_t cut     = 0;

    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(seconds * 1000);
    bool             running  = true;
    while (running && static_cast<int32_t>(deadline - xTaskGetTickCount()) > 0) {
        camera_stream_t stream;
        if (camera_stream_open(&stream, STREAM_WAIT_MS) != 0) break;

        uint32_t size    = 0;
        uint8_t  head[2] = {0, 0};
        uint8_t  tail[2] = {0, 0};
        while (!camera_stream_at_end(&stream)) {
            const int n = camera_stream_read(&stream, chunk, STREAM_CHUNK_BYTES, STREAM_WAIT_MS);
            if (n <= 0) {
                if (n == -ESTALE || n == -EBADMSG) {
                    cut++;
                } else {
                    running = false;
                }
                break;
            }

            for (int i = 0; i < n; i++) {
                if (size + static_cast<uint32_t>(i) < 2) head[size + i] = chunk[i];
                tail[0] = tail[1];
                tail[1] = chunk[i];
            }
            size += static_cast<uint32_t>(n);
        }

        if (!camera_stream_at_end(&stream)) continue;

        frames++;
        bytes += size;
        if (size > biggest) biggest = size;
        if (head[0] != 0xFF || head[1] != 0xD8 || tail[0] != 0xFF || tail[1] != 0xD9) damaged++;
    }

    camera_stats_t stats;
    camera_get_stats(&stats);
    free(chunk);

    printf("frames %lu, cut short %lu, damaged %lu, avg %lu bytes, biggest %lu bytes, in ~%u s\r\n",
           static_cast<unsigned long>(frames), static_cast<unsigned long>(cut), static_cast<unsigned long>(damaged),
           static_cast<unsigned long>(frames ? bytes / frames : 0), static_cast<unsigned long>(biggest), seconds);
    printf("camera: %lu frames, %lu lost\r\n", static_cast<unsigned long>(stats.frames),
           static_cast<unsigned long>(stats.frames_lost));
    return frames > 0 ? 0 : 1;
}

// The frame of capture_frame. It stays here until the next call, so tools/jpeg_over_uart can read it with
// memory_read at the address the command prints.
static uint8_t g_frame_buf[FRAME_BUF_BYTES] __attribute__((aligned(4)));

// Starts the camera on the first call and copies one whole frame.
int command_capture_frame(__unused int argc, __unused const char *argv[]) {
    CameraSession session;
    int           rc = session.ensure_started();
    if (rc == 0) rc = camera_read_frame(g_frame_buf, sizeof(g_frame_buf), FRAME_WAIT_MS);
    if (rc < 0) {
        camera_stats_t stats;
        camera_get_stats(&stats);
        printf("capture failed: %s (encoder reports a frame of %lu bytes)\r\n", camera_strerror(rc),
               static_cast<unsigned long>(stats.last_frame_bytes));
        return 1;
    }

    printf("addr 0x%08lx  size %d\r\n", reinterpret_cast<unsigned long>(g_frame_buf), rc);
    return 0;
}

#endif // CAMERA_I2C
