#include <stdio.h>
#include <stdint.h>
#include "device/camera.h"
#include "hardware/sctrl.h"
#include "platform/unistd.h"
#include "commands.h"
#include "board.h"

#ifdef CAMERA_I2C

// Frames are 12-27 KB depending on the scene.
#define FRAME_BUF_BYTES (48 * 1024)
#define FRAME_MIN_BYTES (24 * 1024)
#define FRAME_MAX_BYTES (47 * 1024)
#define FRAME_WAIT_MS   (2000)

static uint8_t g_frame_buf[FRAME_BUF_BYTES] __attribute__((aligned(4)));

// Full camera start, then a short check that frames really arrive.
int command_capture_start(__unused int argc, __unused const char *argv[]) {
    sctrl_init();
    sctrl_set_cpu_freq_hz(CPU_FREQ_160_MHZ);

    const camera_config_t config = {
        .sensor          = nullptr,
        .frame_min_bytes = FRAME_MIN_BYTES,
        .frame_max_bytes = FRAME_MAX_BYTES,
    };
    const int rc = camera_start(&config);
    if (rc != 0) {
        printf("camera start failed: %s\r\n", camera_strerror(rc));
        return 1;
    }

    camera_info_t info;
    if (camera_get_info(&info) == 0) printf("sensor: %s, %ux%u\r\n", info.sensor, info.width, info.height);

    camera_stats_t before;
    camera_get_stats(&before);
    usleep(500000);
    camera_stats_t after;
    camera_get_stats(&after);
    const uint32_t frames = after.frames - before.frames;

    printf("camera started: %lu frames in ~0.5 s, last frame %lu bytes\r\n", static_cast<unsigned long>(frames),
           static_cast<unsigned long>(after.last_frame_bytes));
    return frames > 0 ? 0 : 1;
}

// Needs a started camera (capture_start).
int command_capture_frame(__unused int argc, __unused const char *argv[]) {
    const int size = camera_read_frame(g_frame_buf, sizeof(g_frame_buf), FRAME_WAIT_MS);
    if (size < 0) {
        camera_stats_t stats;
        camera_get_stats(&stats);
        printf("capture failed: %s (encoder reports a frame of %lu bytes)\r\n", camera_strerror(size),
               static_cast<unsigned long>(stats.last_frame_bytes));
        return 1;
    }

    printf("addr 0x%08lx  size %d\r\n", reinterpret_cast<unsigned long>(g_frame_buf), size);
    return 0;
}

#endif // CAMERA_I2C
