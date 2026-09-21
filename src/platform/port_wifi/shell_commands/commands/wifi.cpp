#include "shell/commands_wifi.h"
#include <stdio.h>
#include <string.h>

#include "subcommands.h"

// wifi scan [iface]
// wifi join <ssid> [--bssid value] [--channel N]
// wifi disconnect [iface]
// wifi ap <ssid> [--password pwd]
// wifi stat [iface]
// wifi ip [iface]

// Its own table rather than Shell::Handler's: a subcommand takes the parent's
// name as well, so the two signatures differ.
static const struct {
    const char *name;
    int (*handler)(const char *command, int argc, const char *argv[]);
    const char *description;
} s_subcommands[] = {
    {"help", nullptr, "this help"},
    {"scan", wifi_sub_scan, "sweep the band and list what answered"},
    {"join", wifi_sub_join, "associate with one open network"},
    {"connect", wifi_sub_join, "alias to join"},
    {"disconnect", wifi_sub_disconnect, "drop the link, keep the station interface"},
    {"sta", wifi_sub_sta, "station interface: sta down"},
    {"ap", wifi_sub_ap, "access point: ap <ssid>, ap down"},
    {"stat", wifi_sub_stat, "where the link is, and what has moved"},
    {"ip", wifi_sub_ip, "addresses, per interface"},
};

static void usage(const char *command) {
    printf("usage: %s <command> [...]\r\n", command);
    for (const auto &sub : s_subcommands) {
        printf("  %-11s %s\r\n", sub.name, sub.description);
    }
    printf("\r\nAny command brings the LMAC up first, so there is no start step.\r\n");
}

int command_wifi(int argc, const char *argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return -1;
    }

    if (strcmp(argv[1], "help") == 0) {
        usage(argv[0]);
        return 0;
    }

    for (const auto &sub : s_subcommands) {
        if (!sub.handler) continue;

        // The subcommand is handed its own name as argv[0], so its usage lines
        // read the same whichever way it was reached.
        if (strcmp(sub.name, argv[1]) == 0) return sub.handler(argv[0], argc - 1, argv + 1);
    }

    printf("unknown command: %s\r\n", argv[1]);
    usage(argv[0]);

    return -1;
}
