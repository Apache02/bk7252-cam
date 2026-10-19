// Associating, from the shell: open networks, and WPA2-Personal with a password.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shell/Parser.h"

#include "subcommands.h"

#include "wifi/sta.h"

// wifi join — associates; the link and DHCP follow on their own. The password is
// a WPA2-Personal passphrase of 8 to 63 characters; without it the join is open.
// Given both bssid and channel the join goes straight out, otherwise a full sweep
// runs first, which takes about a second longer. WPA2 adds about half a second
// for the key and up to ten for the handshake.
int wifi_sub_join(int argc, const char *argv[]) {
    StaConfig config = {};

    for (int i = 1; i < argc; i++) {
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
        } else if (!config.passphrase) {
            config.passphrase = argv[i];
        } else {
            printf("[wifi] unknown option: %s\r\n", argv[i]);
            return -1;
        }
    }

    if (!config.ssid) {
        printf("[wifi] join wants an SSID\r\n");
        return -1;
    }

    printf("[wifi] joining \"%s\"%s\r\n", config.ssid, config.passphrase ? ", WPA2" : "");

    Shared<Promise<ConnectResult>> promise = wifi_sta_start(&config);
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
            printf("[wifi] asking for an address, see wifi info\r\n");
            return 0;
        case Refused:
            printf("[wifi] the AP refused, 802.11 status %u\r\n", result->status_code);
            return -1;
        case NoReply:
            printf("[wifi] no answer before the deadline\r\n");
            return -1;
        case AuthFailed:
            printf("[wifi] the handshake failed: wrong passphrase, or the AP stopped answering\r\n");
            return -1;
    }

    return -1;
}
