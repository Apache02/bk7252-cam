// The station interface, from the shell.

#include <stdio.h>
#include <string.h>

#include "subcommands.h"

#include "wifi/sta.h"

// wifi sta down — leaves the BSS, drops the link, stops DHCP and takes the
// station interface down. A later wifi join brings it up again.
int wifi_sub_sta(int argc, const char *argv[]) {
    if (argc != 2) {
        printf("[wifi] sta wants down\r\n");
        return -1;
    }

    if (strcmp(argv[1], "down") != 0) {
        printf("[wifi] unknown option: %s\r\n", argv[1]);
        return -1;
    }

    if (!wifi_sta_is_up()) {
        printf("[wifi] the station interface is not up\r\n");
        return -1;
    }

    if (!wifi_sta_stop()) {
        printf("[wifi] the LMAC would not drop the link\r\n");
        return -1;
    }

    printf("[wifi] station stopped\r\n");
    return 0;
}
