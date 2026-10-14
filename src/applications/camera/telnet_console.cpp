#include "telnet_console.h"

#include <stdio.h>

#include "platform/stdio.h"
#include "platform/telnet_server.h"

#define TELNET_PORT 23

static constexpr platform_stdio_tap_t TAP = {
    .write = telnet_server_write,
    .read  = telnet_server_read,
};

static void on_connect(__unused void *user) { platform_stdio_set_tap(&TAP); }

static void on_disconnect(__unused void *user) { platform_stdio_set_tap(nullptr); }

void telnet_console_start() {
    const telnet_server_config_t config = {
        .port          = TELNET_PORT,
        .stack_words   = 0,
        .greeting      = "Shell is ready\r\n\r\n> ",
        .on_connect    = on_connect,
        .on_disconnect = on_disconnect,
        .user          = nullptr,
    };

    if (telnet_server_start(&config)) {
        printf("telnet: listening on port %u\r\n", TELNET_PORT);
    } else {
        printf("telnet: could not start\r\n");
    }
}
