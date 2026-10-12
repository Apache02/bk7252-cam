// An open access point, from the shell.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "subcommands.h"

#include "wifi/ap.h"

#define DEFAULT_CHANNEL 6
#define MAX_SSID_LEN    32

static void print_usage(const char *command, const char *name) {
    printf("usage: %s %s <ssid> [--channel N]\r\n", command, name);
    printf("       %s %s down\r\n", command, name);
    printf("  --channel  1..14, default %d\r\n", DEFAULT_CHANNEL);
    printf("\r\n  The AP is open: there is no password. It answers 192.168.4.1/24 and\r\n");
    printf("  gives its clients 192.168.4.2 to 192.168.4.5.\r\n");
    printf("  \"down\" stops a running AP, so it cannot be used as an SSID here.\r\n");
}

// Wi-Fi ap — starts an access point, or stops the running one.
int wifi_sub_ap(const char *command, int argc, const char *argv[]) {
    ApConfig config = {.ssid = nullptr, .channel = DEFAULT_CHANNEL};

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(command, argv[0]);
            return 0;
        }

        if (strcmp(argv[i], "--channel") == 0) {
            char         *end     = nullptr;
            unsigned long channel = (++i < argc) ? strtoul(argv[i], &end, 10) : 0;
            if (!end || *end || channel < 1 || channel > 14) {
                printf("[wifi] --channel wants 1..14\r\n");
                return -1;
            }
            config.channel = static_cast<uint8_t>(channel);
        } else if (!config.ssid) {
            config.ssid = argv[i];
        } else {
            printf("[wifi] unknown option: %s\r\n", argv[i]);
            print_usage(command, argv[0]);
            return -1;
        }
    }

    if (!config.ssid) {
        print_usage(command, argv[0]);
        return -1;
    }

    if (strcmp(config.ssid, "down") == 0) {
        if (!wifi_ap_stop()) {
            printf("[wifi] no access point is running\r\n");
            return -1;
        }
        printf("[wifi] access point stopped\r\n");
        return 0;
    }

    size_t length = strlen(config.ssid);
    if (length < 1 || length > MAX_SSID_LEN) {
        printf("[wifi] the SSID wants 1 to %d bytes\r\n", MAX_SSID_LEN);
        return -1;
    }

    if (!wifi_ap_start(&config)) {
        printf("[wifi] the access point did not start: one may already be running, or the LMAC refused\r\n");
        return -1;
    }

    printf("[wifi] access point \"%s\" is up on channel %u, open, 192.168.4.1\r\n", config.ssid, config.channel);
    return 0;
}
