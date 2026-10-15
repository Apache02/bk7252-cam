// Leaving a BSS, from the shell.

#include <stdio.h>
#include <string.h>

#include "subcommands.h"

#include "wifi/sta.h"

static void print_usage(const char *command, const char *name) {
    printf("usage: %s %s\r\n", command, name);
    printf("\r\n  Leaves the BSS, drops the link and stops DHCP. A later %s join\r\n", command);
    printf("  brings the station interface up again.\r\n");
}

// wifi disconnect — deauthenticates and takes the link down.
int wifi_sub_disconnect(const char *command, int argc, const char *argv[]) {
    if (argc >= 2) {
        if (strcmp(argv[1], "--help") != 0) printf("[wifi] unknown option: %s\r\n", argv[1]);
        print_usage(command, argv[0]);
        return strcmp(argv[1], "--help") == 0 ? 0 : -1;
    }

    if (!wifi_sta_is_up()) {
        printf("[wifi] the station interface is not up\r\n");
        return -1;
    }

    if (!wifi_sta_stop()) {
        printf("[wifi] the LMAC would not drop the link\r\n");
        return -1;
    }

    printf("[wifi] link down\r\n");
    return 0;
}
