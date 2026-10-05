// Leaving a BSS, from the shell.

#include <stdio.h>
#include <string.h>

#include "subcommands.h"

#include "wifi/connect.h"
#include "wifi/net.h"

static void print_usage(const char *command, const char *name) {
    printf("usage: %s %s\r\n", command, name);
    printf("\r\n  Drops the link and stops DHCP. The station interface stays up,\r\n");
    printf("  so a later %s join needs no bring-up.\r\n", command);
}

// wifi disconnect — deauthenticates and takes the link down.
int wifi_sub_disconnect(const char *command, int argc, const char *argv[]) {
    if (argc >= 2) {
        if (strcmp(argv[1], "--help") != 0) printf("[wifi] unknown option: %s\r\n", argv[1]);
        print_usage(command, argv[0]);
        return strcmp(argv[1], "--help") == 0 ? 0 : -1;
    }

    // Looked up rather than created: there is nothing to leave on an interface
    // that was never brought up.
    const struct net_if *iface = wifi_net_iface(WIFI_IFACE_STA);
    if (!iface || !iface->started) {
        printf("[wifi] the station interface is not up\r\n");
        return -1;
    }

    if (!wifi_disconnect(iface->vif)) {
        printf("[wifi] the LMAC would not drop the link\r\n");
        return -1;
    }

    printf("[wifi] link down\r\n");
    return 0;
}
