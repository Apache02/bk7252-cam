#include <stdio.h>
#include <stdint.h>
#include "device/camera.h"
#include "hardware/sctrl.h"
#include "platform/unistd.h"
#include "commands.h"

// Frames are 12-27 KB depending on the scene.
#define FRAME_BUF_BYTES (48 * 1024)

static uint8_t g_frame_buf[FRAME_BUF_BYTES] __attribute__((aligned(4)));

// Full camera start, then a short check that frames really arrive.
int command_capture_start(__unused int argc, __unused const char *argv[]) {
    sctrl_init();
    sctrl_set_cpu_freq_hz(CPU_FREQ_160_MHZ);

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
