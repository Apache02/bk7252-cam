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
#include "app/version.h"
#include "hardware/wdt.h"
#include "wifi/scan.h"
#include "wifi/sta.h"

#define HTTP_PORT    80
#define HTTP_WORKERS 2
#define POLL_MS      5

#define FRAME_MIN_BYTES (24 * 1024)
#define FRAME_MAX_BYTES 47104
#define SLOT_BYTES      (48 * 1024)
#define SLOT_COUNT      2

#define BOUNDARY "frame"

// The built frontend, gzipped, generated from frontend/dist/index.html.gz.
extern "C" const unsigned char page_app[];
extern "C" const unsigned      page_app_size;

static void serve_app(__unused const http_request_t *request, http_response_t *response, __unused void *user) {
    http_response_set_content_type(response, "text/html");
    http_response_set_content_encoding(response, "gzip");
    http_respond_binary(response, page_app, page_app_size);
}

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

struct IpText {
    char text[16];
};

// An address in host byte order as dotted decimal.
static IpText format_ip(const uint32_t address) {
    IpText out;
    snprintf(out.text, sizeof(out.text), "%u.%u.%u.%u", static_cast<unsigned>(address >> 24),
             static_cast<unsigned>((address >> 16) & 0xff), static_cast<unsigned>((address >> 8) & 0xff),
             static_cast<unsigned>(address & 0xff));
    return out;
}

// The station address as a JSON object, or "null" while it has none.
#define NETWORK_JSON_MAX 96

static void format_network(char *out, const size_t size) {
    uint32_t ip, mask, gw;
    if (wifi_sta_get_address(&ip, &mask, &gw)) {
        snprintf(out, size, "{\"ip\": \"%s\", \"mask\": \"%s\", \"gw\": \"%s\"}", format_ip(ip).text,
                 format_ip(mask).text, format_ip(gw).text);
    } else {
        snprintf(out, size, "null");
    }
}

static void serve_system_info(__unused const http_request_t *request, http_response_t *response, __unused void *user) {
    char network[NETWORK_JSON_MAX];
    format_network(network, sizeof(network));

    char buf[192];
    snprintf(buf, sizeof(buf), "{\"version\": \"%s\", \"network\": %s}", get_version(), network);

    http_response_set_content_type(response, "application/json");
    http_respond_string(response, buf);
}

static void set_config(const Config &config) {
    taskENTER_CRITICAL();
    g_config = config;
    taskEXIT_CRITICAL();
}

// How long a new network gets to hand out an address.
#define SETUP_CONNECT_SECONDS 30

// Joins the network named in the body ({"sta_ssid", "sta_password"}) while the access point stays up.
// On success it stores the station config, answers with the new address, and only then takes the
// access point down. On failure the old config comes back and nothing is stored.
static void serve_setup(const http_request_t *request, http_response_t *response, __unused void *user) {
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

    Config backup;
    taskENTER_CRITICAL();
    backup = g_config;
    taskEXIT_CRITICAL();

    Config trial = backup;
    const bool ok = http_request_read_body(request, buf, length) == static_cast<int>(length) &&
                    config_from_json(buf, length, trial) && trial.sta_ssid[0] != '\0';
    vPortFree(buf);
    if (!ok) {
        respond_error(response, 400, "invalid ssid or password");
        return;
    }
    trial.sta_enabled = true;

    if (backup.sta_enabled) {
        // A station that is already up on another network has to be taken down first.
        Config off       = backup;
        off.sta_enabled = false;
        set_config(off);
        main_task_config_changed();
        vTaskDelay(pdMS_TO_TICKS(1000));
        wifi_sta_stop();
    }

    set_config(trial);
    main_task_config_changed();

    for (int i = 0; i < SETUP_CONNECT_SECONDS; i++) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (!wifi_sta_get_address(nullptr, nullptr, nullptr)) continue;

        trial.ap_enabled = false;
        set_config(trial);
        const int error = config_save();
        if (error) {
            printf("[http] setup not stored: %d\r\n", error);
            trial.ap_enabled = true;
            set_config(trial);
            respond_error(response, 500, "connected but not stored");
            return;
        }

        char network[NETWORK_JSON_MAX];
        format_network(network, sizeof(network));
        char body[NETWORK_JSON_MAX + 40];
        snprintf(body, sizeof(body), "{\"success\": true, \"network\": %s}", network);

        http_response_set_content_type(response, "application/json");
        http_respond_string(response, body);

        // Gives the answer time to reach the client before the access point goes down.
        vTaskDelay(pdMS_TO_TICKS(2000));
        main_task_config_changed();
        return;
    }

    wifi_sta_stop();
    vTaskDelay(pdMS_TO_TICKS(1000));
    set_config(backup);
    main_task_config_changed();
    vTaskDelay(pdMS_TO_TICKS(1000));

    respond_error(response, 504, "could not connect");
}

// The scan JSON with room for 24 networks at their longest.
#define SCAN_JSON_MAX 4096

// Sweeps every channel and answers with the networks found. Blocks this worker for the whole sweep.
static void serve_scan(__unused const http_request_t *request, http_response_t *response, __unused void *user) {
    char *buf = static_cast<char *>(pvPortMalloc(SCAN_JSON_MAX));
    if (!buf) {
        respond_error(response, 500, "out of memory");
        return;
    }

    Shared<Promise<ScanResult>> promise = wifi_scan(nullptr);
    if (promise.empty()) {
        respond_error(response, 503, "scan not started");
        vPortFree(buf);
        return;
    }

    Owned<ScanResult> result = promise->await();
    if (result.empty()) {
        respond_error(response, 500, "scan produced no result");
        vPortFree(buf);
        return;
    }

    const size_t length = scan_to_json(*result, buf, SCAN_JSON_MAX);
    if (length == 0) {
        respond_error(response, 500, "scan does not fit");
    } else {
        http_response_set_content_type(response, "application/json");
        http_respond_binary(response, buf, static_cast<uint32_t>(length));
    }
    vPortFree(buf);
}

static void serve_system_reboot(__unused const http_request_t *request, http_response_t *response, __unused void *user) {
    http_response_set_status(response, 200);
    http_response_set_content_type(response, "application/json");
    http_respond_string(response, "{\"success\": true}");

    printf("\r\nreboot system\r\n");
    vTaskDelay(pdMS_TO_TICKS(200));

    taskENTER_CRITICAL();
    wdt_reboot(1000);
    taskEXIT_CRITICAL();
}

static void serve_system_reset(__unused const http_request_t *request, http_response_t *response, __unused void *user) {
    const auto success = config_reset() == 0;
    vTaskDelay(pdMS_TO_TICKS(1000));

    http_response_set_status(response, 200);
    http_response_set_content_type(response, "application/json");
    http_respond_string(response, success ? "{\"success\": true}" : "{\"success\": false}");

    vTaskDelay(pdMS_TO_TICKS(200));

    main_task_config_changed();
}

void camera_web_start() {
    static constexpr http_route_t routes[] = {
        {"GET", "/favicon.ico", http_default_404, nullptr},
        // pages
        {"GET", "/", serve_app, nullptr},
        {"GET", "/camera", serve_app, nullptr},
        {"GET", "/system", serve_app, nullptr},
        {"GET", "/join", serve_app, nullptr},
        // api
        {"GET", "/settings.json", serve_settings_get, nullptr},
        {"POST", "/settings", serve_settings_post, nullptr},
        {"GET", "/system.json", serve_system_info, nullptr},
        {"GET", "/scan.json", serve_scan, nullptr},
        {"POST", "/setup", serve_setup, nullptr},
        {"POST", "/reboot", serve_system_reboot, nullptr},
        {"POST", "/reset", serve_system_reset, nullptr},
        // camera stream
        {"GET", "/stream", serve_stream, &g_camera},
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
