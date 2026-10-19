#include "shell/commands_wifi.h"
#include <stdio.h>
#include <string.h>

#include "subcommands.h"

// Its own table rather than Shell::Handler's: a subcommand is picked by the
// word after the command's name and gets argv starting at that word.
static const struct {
    const char *name;
    int (*handler)(int argc, const char *argv[]);
} s_subcommands[] = {
    {"scan", wifi_sub_scan},
    {"join", wifi_sub_join},
    {"sta", wifi_sub_sta},
    {"ap", wifi_sub_ap},
    {"info", wifi_sub_info},
};

// One line per way of calling a subcommand, so `ap` and `sta` appear twice.
static const struct {
    const char *usage;
    const char *description;
} s_help[] = {
    {"help", "show this help"},
    {"scan [--long] [--survey]", "list the networks in range"},
    {"ap down", "stop the access point"},
    {"ap <ssid> [--channel N]", "start an open access point"},
    {"sta down", "leave the network, stop the station"},
    {"join <ssid> [password] [--bssid bssid] [--channel N]", "join a network, open or WPA2"},
    {"info [ap|sta]", "link, address and counters"},
};

#define DESCRIPTION_COLUMN 36

static void usage(const char *command) {
    printf("usage: %s <command> [...]\r\n", command);
    printf("commands:\r\n");
    for (const auto &line : s_help) {
        int width = printf("  %s %s", command, line.usage);
        if (width >= DESCRIPTION_COLUMN) {
            printf("\r\n");
            width = 0;
        }
        printf("%*s%s\r\n", DESCRIPTION_COLUMN - width, "", line.description);
    }
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
        if (strcmp(sub.name, argv[1]) == 0) return sub.handler(argc - 1, argv + 1);
    }

    printf("unknown command: %s\r\n", argv[1]);
    usage(argv[0]);

    return -1;
}
