#include "camera_session.h"

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>
#include <errno.h>
#include <stdint.h>
#include "device/camera.h"

#define count_of(x) (sizeof(x) / sizeof(x[0]))

#define IDLE_STOP_MS   10000
#define IDLE_POLL_MS   1000
#define START_ATTEMPTS 2

// Frames are 12-27 KB depending on the scene.
#define FRAME_BUF_BYTES (48 * 1024)

static uint8_t g_frame_buf[FRAME_BUF_BYTES] __attribute__((aligned(4)));

static SemaphoreHandle_t g_lock;
static StaticSemaphore_t g_lock_storage;
static StaticTask_t      g_idle_tcb;
static StackType_t       g_idle_stack[256];

static bool       g_started;
static TickType_t g_last_use;

// Skips a round while a command holds the lock: that command is using the camera.
static void idle_stop_task(__unused void *params) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(IDLE_POLL_MS));
        if (xSemaphoreTake(g_lock, 0) != pdTRUE) continue;

        if (g_started && xTaskGetTickCount() - g_last_use >= pdMS_TO_TICKS(IDLE_STOP_MS)) {
            camera_stop();
            g_started = false;
        }
        xSemaphoreGive(g_lock);
    }
}

CameraSession::CameraSession() {
    if (g_lock == nullptr) {
        g_lock = xSemaphoreCreateMutexStatic(&g_lock_storage);
        xTaskCreateStatic(idle_stop_task, "camera_idle", count_of(g_idle_stack), nullptr, tskIDLE_PRIORITY + 1,
                          g_idle_stack, &g_idle_tcb);
    }
    xSemaphoreTake(g_lock, portMAX_DELAY);
}

CameraSession::~CameraSession() {
    g_last_use = xTaskGetTickCount();
    xSemaphoreGive(g_lock);
}

int CameraSession::ensure_started() {
    if (g_started) return 0;

    const camera_config_t config = {
        .sensor          = nullptr,
        .frame_buf       = g_frame_buf,
        .frame_buf_size  = sizeof(g_frame_buf),
        .frame_min_bytes = sizeof(g_frame_buf) / 2,
        .frame_max_bytes = sizeof(g_frame_buf) - 1024,
    };

    // The first start after a reset often fails with -EIO and the next one works; the cause is not found.
    int rc = 0;
    for (unsigned attempt = 0; attempt < START_ATTEMPTS; attempt++) {
        rc = camera_start(&config);
        if (rc != -EIO) break;
    }

    g_started = rc == 0;
    return rc;
}
