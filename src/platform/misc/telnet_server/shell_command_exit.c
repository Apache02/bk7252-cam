#include <stdio.h>

#include "platform/telnet_server.h"

#include "shell/commands_telnet.h"

int command_telnet_exit(__unused int argc, __unused const char *argv[]) {
    if (!telnet_server_disconnect()) {
        printf("no Telnet client is connected\r\n");
        return -1;
    }

    return 0;
}
