// One connection at a time, one fixed page, no routing, no chunking. Enough
// to prove the stack is up and reachable.

#include "http_server.h"

#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/sockets.h"


#define HTTP_PORT        80
#define REQUEST_BUF_SIZE 256
#define RESPONSE_BUF_SIZE 256

static const char BODY[] = "<html><body><h1>BK7252 cam is alive</h1></body></html>";

static void serve_one(int client_fd) {
    struct timeval timeout;
    timeout.tv_sec  = 2;
    timeout.tv_usec = 0;
    lwip_setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    // The request itself is never parsed — every request gets the same page —
    // this just drains it so the client's send() doesn't fail on a reset.
    char request[REQUEST_BUF_SIZE];
    if (lwip_recv(client_fd, request, sizeof(request), 0) <= 0) {
        lwip_close(client_fd);
        return;
    }

    char response[RESPONSE_BUF_SIZE];
    int  response_len = snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        static_cast<unsigned>(sizeof(BODY) - 1), BODY);

    lwip_send(client_fd, response, response_len, 0);
    lwip_close(client_fd);
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

    if (lwip_bind(listen_fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0 || lwip_listen(listen_fd, 1) < 0) {
        printf("http: bind/listen failed\r\n");
        lwip_close(listen_fd);
        vTaskDelete(nullptr);
        return;
    }

    printf("http: listening on port %u\r\n", HTTP_PORT);

    for (;;) {
        int client_fd = lwip_accept(listen_fd, nullptr, nullptr);
        if (client_fd < 0) continue;
        serve_one(client_fd);
    }
}

static StaticTask_t httpTaskTCB;
static StackType_t  httpTaskStack[configMINIMAL_STACK_SIZE * 4];

void http_server_start() {
    // clang-format off
    xTaskCreateStatic(
        vTaskHttp,
        "http",
        sizeof(httpTaskStack) / sizeof(httpTaskStack[0]),
        nullptr,
        configMAX_PRIORITIES - 2,
        httpTaskStack,
        &httpTaskTCB
    );
    // clang-format on
}
