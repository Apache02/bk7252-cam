// Associating, from the shell. Open networks only: the port links no
// supplicant, so wifi/connect.h has no way to carry a key.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shell/Parser.h"

#include "subcommands.h"

#include "wifi/connect.h"
#include "wifi/net.h"

static void print_usage(const char *command, const char *name) {
    printf("usage: %s %s <ssid> [--bssid xx:xx:xx:xx:xx:xx] [--channel N]\r\n", command, name);
    printf("  --bssid    the AP to join, when it is already known\r\n");
    printf("  --channel  1..14, the channel that AP is on\r\n");
    printf("\r\n  Given both, the join goes straight out. Given neither, a full sweep\r\n");
    printf("  runs first to find them, so this blocks for about a second longer.\r\n");
    printf("\r\n  Open networks only: nothing here can carry a key.\r\n");
}

// wifi join — associates, then hands the interface to DHCP.
int wifi_sub_join(const char *command, int argc, const char *argv[]) {
    ConnectConfig config = {};

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(command, argv[0]);
            return 0;
        }

        if (strcmp(argv[i], "--bssid") == 0) {
            if (++i >= argc) {
                printf("[wifi] --bssid wants six hex pairs, like c8:47:8c:42:88:48\r\n");
                return -1;
            }

            auto bssid = take_mac(argv[i]);
            if (bssid.is_err()) {
                printf("[wifi] --bssid wants six hex pairs, like c8:47:8c:42:88:48\r\n");
                return -1;
            }
            memcpy(config.bssid, bssid.r.addr, sizeof(config.bssid));
        } else if (strcmp(argv[i], "--channel") == 0) {
            char         *end     = nullptr;
            unsigned long channel = (++i < argc) ? strtoul(argv[i], &end, 10) : 0;
            if (!end || *end || channel < 1 || channel > 14) {
                printf("[wifi] --channel wants 1..14\r\n");
                return -1;
            }
            config.channel = (uint8_t)channel;
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

    // Parsing comes first, so a typo does not bring the LMAC up.
    struct net_if *iface = wifi_net_ensure_iface(WIFI_IFACE_STA);
    if (!iface) {
        printf("[wifi] the station interface could not be brought up\r\n");
        return -1;
    }

    printf("[wifi] joining \"%s\"\r\n", config.ssid);

    Shared<Promise<ConnectResult>> promise = wifi_connect_start(iface->vif, &config);
    if (promise.empty()) {
        printf("[wifi] association not started\r\n");
        return -1;
    }

    // Blocks until the LMAC says how it went or the attempt is given up on.
    Owned<ConnectResult> result = promise->await();
    if (result.empty()) {
        printf("[wifi] association produced no result\r\n");
        return -1;
    }

    switch (result->status) {
        case Associated:
            printf("[wifi] associated with %02x:%02x:%02x:%02x:%02x:%02x\r\n", result->bssid[0], result->bssid[1],
                   result->bssid[2], result->bssid[3], result->bssid[4], result->bssid[5]);
            wifi_net_link_up(result->vif_idx);
            printf("[wifi] asking for an address, see %s ip\r\n", command);
            return 0;
        case Refused:
            printf("[wifi] the AP refused, 802.11 status %u\r\n", result->status_code);
            return -1;
        case NoReply:
            printf("[wifi] no answer before the deadline\r\n");
            return -1;
    }

    return -1;
}
