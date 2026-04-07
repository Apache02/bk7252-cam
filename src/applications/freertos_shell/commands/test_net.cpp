#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include <string.h>
#include "lwip/sockets.h"


#define TEST_PORT       7777
#define TEST_MESSAGE    "ping"


__unused static void udp_echo_server_task(void *arg);

__unused static void udp_client_task(void *arg);


int command_test_net(__unused int argc, __unused const char *argv[]) {
    xTaskCreate(udp_echo_server_task, "udp_srv", 2048, NULL, 2, NULL);
    xTaskCreate(udp_client_task, "udp_cli", 2048, NULL, 2, NULL);

    printf("tasks created\r\n");

    return 0;
}


static void udp_echo_server_task(void *arg) {
    printf("[net_test] server: started\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("[net_test] server: socket failed\r\n");
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(TEST_PORT),
        .sin_addr = {
            .s_addr = htonl(INADDR_ANY)
        },
    };

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("[net_test] server: bind failed\r\n");
        closesocket(sock);
        vTaskDelete(NULL);
        return;
    }

    printf("[net_test] server: listening on port %d\r\n", TEST_PORT);

    char buf[64];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (1) {
        int len = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                           (struct sockaddr *)&client_addr, &client_len);
        if (len > 0) {
            buf[len] = '\0';
            printf("[net_test] server: received '%s'\r\n", buf);
            sendto(sock, buf, len, 0,
                   (struct sockaddr *)&client_addr, client_len);
        }
    }
}

static void udp_client_task(void *arg) {
    printf("[net_test] client: started\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));

    vTaskDelay(pdMS_TO_TICKS(500));

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("[net_test] client: socket failed\r\n");
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(TEST_PORT),
        .sin_addr = {
            .s_addr = htonl(INADDR_LOOPBACK), /* 127.0.0.1 */
        },
    };

    const char *msg = TEST_MESSAGE;
    sendto(sock, msg, strlen(msg), 0,
           (struct sockaddr *)&server_addr, sizeof(server_addr));
    printf("[net_test] client: sent '%s'\r\n", msg);

    char buf[64];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);

    int len = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                       (struct sockaddr *)&from_addr, &from_len);
    if (len > 0) {
        buf[len] = '\0';
        printf("[net_test] client: echo received '%s' -- LOOPBACK OK\r\n", buf);
    } else {
        printf("[net_test] client: no response -- FAILED\r\n");
    }

    closesocket(sock);
    vTaskDelete(NULL);
}
