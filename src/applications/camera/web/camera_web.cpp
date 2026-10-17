#include "app/camera_web.h"

#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "platform/http_server.h"
#include "platform/http_server_defaults.h"

#include "device/camera.h"

#include "app/config.h"
#include "app/config_json.h"
#include "app/main_task.h"

#define HTTP_PORT    80
#define HTTP_WORKERS 2
#define POLL_MS      5

#define FRAME_MIN_BYTES (24 * 1024)
#define FRAME_MAX_BYTES 47104
#define SLOT_BYTES      (48 * 1024)
#define SLOT_COUNT      2

#define BOUNDARY "frame"

static constexpr char PAGE_CAMERA[] =
    "<html><body style=\"background:white; display:flex; justify-content:center; align-items:center;\">"
    "<img src=\"/stream\" style=\"width:640px; height:480px; margin:1em;\">"
    "</body></html>";

// The built frontend, NUL-terminated, generated from frontend/dist/index.html.
extern "C" const char page_home[];

struct Camera {
    // The encoder only needs this while a frame is being built; the stream uses slots instead.
    alignas(4) uint8_t frame_buf[64];

    // The frames of a stream. Only the one request that holds `streaming` uses them.
    alignas(8) uint8_t slots[SLOT_BYTES * SLOT_COUNT];

    bool started;
    bool streaming;
};

static Camera g_camera;

static bool ensure_camera(Camera *camera) {
    if (camera->started) return true;

    const camera_config_t config = {
        .sensor          = nullptr,
        .frame_buf       = camera->frame_buf,
        .frame_buf_size  = sizeof(camera->frame_buf),
        .frame_min_bytes = FRAME_MIN_BYTES,
        .frame_max_bytes = FRAME_MAX_BYTES,
    };
    const int rc = camera_start(&config);
    if (rc != 0) {
        printf("http: camera start failed: %s\r\n", camera_strerror(rc));
        return false;
    }
    camera->started = true;
    return true;
}

// The camera has one frame source, so only one request may stream from it.
static bool claim_stream(Camera *camera) {
    taskENTER_CRITICAL();
    const bool free   = !camera->streaming;
    camera->streaming = true;
    taskEXIT_CRITICAL();
    return free;
}

static void release_stream(Camera *camera) {
    taskENTER_CRITICAL();
    camera->streaming = false;
    taskEXIT_CRITICAL();
}

// Takes the newest finished frame and gives back any older ones, so a slow client sees
// the present instead of a growing delay.
static bool take_latest(camera_frame_t *frame) {
    if (camera_stream_get(frame) != 0) return false;

    camera_frame_t newer;
    while (camera_stream_get(&newer) == 0) {
        camera_stream_release(frame);
        *frame = newer;
    }
    return true;
}

static void stream_frames(http_response_t *response) {
    uint32_t         sent    = 0;
    const TickType_t started = xTaskGetTickCount();

    for (;;) {
        camera_frame_t frame;
        if (!take_latest(&frame)) {
            vTaskDelay(pdMS_TO_TICKS(POLL_MS));
            continue;
        }

        char      part[96];
        const int len = snprintf(part, sizeof(part),
                                 "--" BOUNDARY "\r\n"
                                 "Content-Type: image/jpeg\r\n"
                                 "Content-Length: %u\r\n"
                                 "\r\n",
                                 static_cast<unsigned>(frame.size));

        http_response_set_more(response, true);
        bool ok = http_respond_binary(response, part, len) && http_respond_binary(response, frame.data, frame.size);
        http_response_set_more(response, false);
        ok = ok && http_respond_string(response, "\r\n");

        camera_stream_release(&frame);
        if (!ok) break;
        sent++;
    }

    const uint32_t elapsed_ms = pdTICKS_TO_MS(static_cast<uint32_t>(xTaskGetTickCount() - started));
    printf("http: stream closed after %lu.%03lu s, %lu frames sent, %lu dropped\r\n", elapsed_ms / 1000,
           elapsed_ms % 1000, sent, camera_stream_dropped());
}

static void serve_stream(__unused const http_request_t *request, http_response_t *response, void *user) {
    auto *camera = static_cast<Camera *>(user);

    if (!claim_stream(camera)) {
        http_response_set_status(response, 503);
        http_respond_string(response, "");
        return;
    }

    if (ensure_camera(camera)) {
        const camera_stream_config_t config = {
            .slots      = camera->slots,
            .slot_size  = SLOT_BYTES,
            .slot_count = SLOT_COUNT,
            .on_frame   = nullptr,
        };
        const int rc = camera_stream_start(&config);
        if (rc == 0) {
            http_response_set_content_type(response, "multipart/x-mixed-replace; boundary=" BOUNDARY);
            http_response_omit_content_length(response);
            stream_frames(response);
            camera_stream_stop();
        } else {
            printf("http: stream start failed: %s\r\n", camera_strerror(rc));
        }
    }

    release_stream(camera);
}

// The config as JSON, with room for every field written at its longest.
#define CONFIG_JSON_MAX 1024

static void respond_error(http_response_t *response, int status, const char *message) {
    http_response_set_status(response, status);
    http_response_set_content_type(response, "text/plain");
    http_respond_string(response, message);
}

static void respond_config(http_response_t *response, char *buf) {
    Config snapshot;
    taskENTER_CRITICAL();
    snapshot = g_config;
    taskEXIT_CRITICAL();

    const size_t length = config_to_json(snapshot, buf, CONFIG_JSON_MAX);
    if (length == 0) {
        respond_error(response, 500, "config does not fit");
        return;
    }

    http_response_set_content_type(response, "application/json");
    http_respond_binary(response, buf, static_cast<uint32_t>(length));
}

static void serve_settings_get(__unused const http_request_t *request, http_response_t *response, __unused void *user) {
    char *buf = static_cast<char *>(pvPortMalloc(CONFIG_JSON_MAX));
    if (!buf) {
        respond_error(response, 500, "out of memory");
        return;
    }

    respond_config(response, buf);
    vPortFree(buf);
}

// Replaces g_config with the body, stores it, and tells the main task.
static void serve_settings_post(const http_request_t *request, http_response_t *response, __unused void *user) {
    const uint32_t length = request->content_length;
    if (length == 0) {
        respond_error(response, 400, "empty body");
        return;
    }
    if (length >= CONFIG_JSON_MAX) {
        respond_error(response, 413, "body too large");
        return;
    }

    char *buf = static_cast<char *>(pvPortMalloc(CONFIG_JSON_MAX));
    if (!buf) {
        respond_error(response, 500, "out of memory");
        return;
    }

    Config next;
    taskENTER_CRITICAL();
    next = g_config;
    taskEXIT_CRITICAL();

    if (http_request_read_body(request, buf, length) != static_cast<int>(length) ||
        !config_from_json(buf, length, next)) {
        respond_error(response, 400, "invalid config");
        vPortFree(buf);
        return;
    }

    taskENTER_CRITICAL();
    g_config = next;
    taskEXIT_CRITICAL();

    main_task_config_changed();

    const int error = config_save();
    if (error) {
        printf("http: config not stored: %d\r\n", error);
        respond_error(response, 500, "config applied but not stored");
    } else {
        respond_config(response, buf);
    }
    vPortFree(buf);
}

void camera_web_start() {
    static constexpr http_route_t routes[] = {
        {"GET", "/", http_default_static_page, const_cast<char *>(page_home)},
        {"GET", "/camera", http_default_static_page, const_cast<char *>(PAGE_CAMERA)},
        {"GET", "/favicon.ico", http_default_404, nullptr},
        {"GET", "/stream", serve_stream, &g_camera},
        {"GET", "/settings.json", serve_settings_get, nullptr},
        {"POST", "/settings", serve_settings_post, nullptr},
        {nullptr, nullptr, nullptr, nullptr},
    };

    constexpr http_server_config_t config = {
        .port        = HTTP_PORT,
        .workers     = HTTP_WORKERS,
        .stack_words = (configMINIMAL_STACK_SIZE * 6),
        .handler     = http_default_handler,
        .user        = const_cast<http_route_t *>(routes),
    };

    if (http_server_start(&config)) {
        printf("http: listening on port %u\r\n", HTTP_PORT);
    } else {
        printf("http: could not start\r\n");
    }
}
