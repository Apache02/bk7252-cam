#include "app/telnet_console.h"

#include <stdio.h>

#include "platform/stdio.h"
#include "platform/telnet_server.h"


static constexpr platform_stdio_tap_t TAP = {
    .write = telnet_server_write,
    .read  = telnet_server_read,
};

bool telnet_console_start(const uint16_t port) {
    const telnet_server_config_t config = {
        .port          = port,
        .stack_words   = 0,
        .greeting      = "Shell is ready\r\n\r\n> ",
        .on_connect    = [](__unused void *user) {
            platform_stdio_set_tap(&TAP);
        },
        .on_disconnect = [](__unused void *user) {
            platform_stdio_set_tap(nullptr);
        },
        .user          = nullptr,
    };

    return telnet_server_start(&config);
}
