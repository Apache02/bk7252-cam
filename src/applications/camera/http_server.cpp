#include "http_server.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/sockets.h"

#include "device/camera.h"


#define HTTP_PORT         80
#define REQUEST_BUF_SIZE  256
#define SEND_TIMEOUT_S    5
#define POLL_MS           5

#define FRAME_MIN_BYTES   (24 * 1024)
#define FRAME_MAX_BYTES   47104
#define SLOT_BYTES        (48 * 1024)
#define SLOT_COUNT        2

#define BOUNDARY          "frame"

static const char PAGE[] =
    "<html><body style=\"margin:0;background:#000\">"
    "<img src=\"/stream\" style=\"width:100%\">"
    "</body></html>";

// The encoder only needs this while a frame is being built; the stream uses slots instead.
static uint8_t g_frame_buf[64] __attribute__((aligned(4)));

static bool g_camera_started;

static bool send_all(int fd, const void *data, size_t size, int flags = 0) {
    return lwip_send(fd, data, size, flags) == static_cast<ssize_t>(size);
}

static void serve_page(int fd) {
    char header[160];
    const int len = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "\r\n",
        static_cast<unsigned>(sizeof(PAGE) - 1));
    if (send_all(fd, header, len, MSG_MORE)) send_all(fd, PAGE, sizeof(PAGE) - 1);
}

static bool ensure_camera() {
    if (g_camera_started) return true;

    const camera_config_t config = {
        .sensor          = nullptr,
        .frame_buf       = g_frame_buf,
        .frame_buf_size  = sizeof(g_frame_buf),
        .frame_min_bytes = FRAME_MIN_BYTES,
        .frame_max_bytes = FRAME_MAX_BYTES,
    };
    const int rc = camera_start(&config);
    if (rc != 0) {
        printf("http: camera start failed: %s\r\n", camera_strerror(rc));
        return false;
    }
    g_camera_started = true;
    return true;
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

static void serve_stream(int fd) {
    if (!ensure_camera()) return;

    auto *slots = static_cast<uint8_t *>(malloc(SLOT_BYTES * SLOT_COUNT));
    if (slots == nullptr) {
        printf("http: no memory for stream slots\r\n");
        return;
    }

    const camera_stream_config_t config = {
        .slots      = slots,
        .slot_size  = SLOT_BYTES,
        .slot_count = SLOT_COUNT,
        .on_frame   = nullptr,
    };
    const int rc = camera_stream_start(&config);
    if (rc != 0) {
        printf("http: stream start failed: %s\r\n", camera_strerror(rc));
        free(slots);
        return;
    }

    static const char HEADER[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=" BOUNDARY "\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n"
        "\r\n";

    uint32_t sent = 0;
    const TickType_t started = xTaskGetTickCount();
    if (send_all(fd, HEADER, sizeof(HEADER) - 1)) {
        for (;;) {
            camera_frame_t frame;
            if (!take_latest(&frame)) {
                vTaskDelay(pdMS_TO_TICKS(POLL_MS));
                continue;
            }

            char part[96];
            const int len = snprintf(part, sizeof(part),
                "--" BOUNDARY "\r\n"
                "Content-Type: image/jpeg\r\n"
                "Content-Length: %u\r\n"
                "\r\n",
                static_cast<unsigned>(frame.size));

            const bool ok = send_all(fd, part, len, MSG_MORE)
                         && send_all(fd, frame.data, frame.size, MSG_MORE)
                         && send_all(fd, "\r\n", 2);
            camera_stream_release(&frame);
            if (!ok) break;
            sent++;
        }
    }

    const uint32_t elapsed_ms = pdTICKS_TO_MS(static_cast<uint32_t>(xTaskGetTickCount() - started));
    const uint32_t dropped    = camera_stream_dropped();
    camera_stream_stop();
    free(slots);
    printf("http: stream closed after %lu.%03lu s, %lu frames sent, %lu dropped\r\n",
           elapsed_ms / 1000, elapsed_ms % 1000, sent, dropped);
}

// Reads the whole request head up to the blank line and keeps what fits of its start. Closing
// a socket with unread data makes lwIP send a reset, and browsers drop the response on it.
static bool read_request(int fd, char *buf, size_t size) {
    size_t kept  = 0;
    unsigned seen = 0; // how much of "\r\n\r\n" has matched so far

    for (;;) {
        char chunk[128];
        const int n = lwip_recv(fd, chunk, sizeof(chunk), 0);
        if (n <= 0) return false;

        for (int i = 0; i < n; i++) {
            if (kept < size - 1) buf[kept++] = chunk[i];

            const char expected = (seen % 2 == 0) ? '\r' : '\n';
            if (chunk[i] == expected) {
                if (++seen == 4) {
                    buf[kept] = '\0';
                    return true;
                }
            } else {
                seen = (chunk[i] == '\r') ? 1 : 0;
            }
        }
    }
}

static void serve_one(int fd) {
    struct timeval timeout;
    timeout.tv_sec  = SEND_TIMEOUT_S;
    timeout.tv_usec = 0;
    lwip_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    lwip_setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    char request[REQUEST_BUF_SIZE];
    if (!read_request(fd, request, sizeof(request))) {
        lwip_close(fd);
        return;
    }

    if (strncmp(request, "GET /stream", 11) == 0) {
        serve_stream(fd);
    } else if (strncmp(request, "GET / ", 6) != 0) {
        static const char NOT_FOUND[] =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n"
            "\r\n";
        send_all(fd, NOT_FOUND, sizeof(NOT_FOUND) - 1);
    } else {
        serve_page(fd);
    }
    lwip_close(fd);
}

static void vTaskHttp(__unused void *pvParams) {
    int listen_fd = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_fd < 0) {
        printf("http: could not open listen socket\r\n");
        vTaskDelete(nullptr);
        return;
    }

    int reuse = 1;
    lwip_setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr = {};
    addr.sin_len            = sizeof(addr);
    addr.sin_family         = AF_INET;
    addr.sin_port           = lwip_htons(HTTP_PORT);

    if (lwip_bind(listen_fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0 || lwip_listen(listen_fd, 2) < 0) {
        printf("http: bind/listen failed\r\n");
        lwip_close(listen_fd);
        vTaskDelete(nullptr);
        return;
    }

    printf("http: listening on port %u\r\n", HTTP_PORT);

    for (;;) {
        const int client_fd = lwip_accept(listen_fd, nullptr, nullptr);
        if (client_fd < 0) continue;
        serve_one(client_fd);
    }
}

static StaticTask_t httpTaskTCB;
static StackType_t  httpTaskStack[configMINIMAL_STACK_SIZE * 8];

void http_server_start() {
    // clang-format off
    xTaskCreateStatic(
        vTaskHttp,
        "http",
        sizeof(httpTaskStack) / sizeof(httpTaskStack[0]),
        nullptr,
        configMAX_PRIORITIES - 3,
        httpTaskStack,
        &httpTaskTCB
    );
    // clang-format on
}
