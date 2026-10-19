// An open access point, from the shell.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "subcommands.h"

#include "wifi/ap.h"

#define DEFAULT_CHANNEL 6
#define MAX_SSID_LEN    32

// wifi ap — starts an access point, or stops the running one. The AP is open, with
// no password. It answers 192.168.4.1/24 and gives its clients 192.168.4.2 to
// 192.168.4.5. The channel defaults to DEFAULT_CHANNEL. "down" stops a running AP,
// so it cannot be used as an SSID here.
int wifi_sub_ap(int argc, const char *argv[]) {
    ApConfig config = {.ssid = nullptr, .channel = DEFAULT_CHANNEL};

    for (int i = 1; i < argc; i++) {
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
            return -1;
        }
    }

    if (!config.ssid) {
        printf("[wifi] ap wants an SSID, or down\r\n");
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
