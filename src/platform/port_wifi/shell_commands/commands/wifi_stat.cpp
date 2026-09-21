// Two reports, neither of which brings anything up: wifi_net_iface() looks an
// interface up without creating it, so "not brought up" is something these can
// print rather than quietly fix.

#include <stdio.h>
#include <string.h>

#include "subcommands.h"

#include "lwip/dhcp.h"

#include "libip/vif.h"
#include "wifi/core.h"
#include "wifi/net.h"
#include "wifi/station_status.h"

#undef count_of
#define count_of(x) (sizeof(x) / sizeof(x[0]))

#define FIELD "  %-13s"

static const char *const iface_names[] = {WIFI_IFACE_STA, WIFI_IFACE_AP};

static const char *station_status_name(msg_sta_states status) {
    switch (status) {
        case MSG_IDLE:
            return "idle";
        case MSG_CONNECTING:
            return "connecting";
        case MSG_PASSWD_WRONG:
            return "wrong password";
        case MSG_NO_AP_FOUND:
            return "no AP found";
        case MSG_CONN_FAIL:
            return "failed";
        case MSG_CONN_SUCCESS:
            return "associated";
        case MSG_GOT_IP:
            return "got an address";
    }
    return "unknown";
}

static void print_mac(const char *label, const uint8_t mac[6]) {
    printf(FIELD "%02x:%02x:%02x:%02x:%02x:%02x\r\n", label, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void print_ipv4(const char *label, const ip4_addr_t *ip) {
    printf(FIELD "%u.%u.%u.%u\r\n", label, (unsigned)ip4_addr1(ip), (unsigned)ip4_addr2(ip), (unsigned)ip4_addr3(ip),
           (unsigned)ip4_addr4(ip));
}

// ---- Reports ---------------------------------------------------------------

static void print_iface_stat(const struct net_if *iface) {
    printf("%s\r\n", iface->name);

    if (!iface->started) {
        printf(FIELD "%s\r\n", "state", "not brought up");
        return;
    }

    printf(FIELD "%s\r\n", "link", netif_is_link_up(&iface->netif) ? "up" : "down");
    printf(FIELD "%u\r\n", "vif", (unsigned)iface->vif);
    print_mac("mac", iface->netif.hwaddr);

    // The archive's own view from here down, kept apart because the two can
    // disagree: a MAC reset drops the LMAC back to idle without telling us.
    const struct vif_info_tag *vif = &vif_info_tab[iface->vif];
    printf(FIELD "%s\r\n", "registered", vif->active ? "yes" : "no");
    print_mac("bssid", (const uint8_t *)&vif->bssid);

    if (iface->type != VIF_STA) return;

    if (iface->type == VIF_STA) {
        printf(FIELD "%s\r\n", "status", station_status_name(mhdr_get_station_status()));
        printf(FIELD "%d dBm\r\n", "rssi", vif->u.sta.rssi);
        printf(FIELD "%u\r\n", "beacon loss", (unsigned)vif->u.sta.beacon_loss_cnt);
        printf(FIELD "%u\r\n", "probe fail", (unsigned)vif->u.sta.mm_retry);
    }

    printf(FIELD "%lu in, %lu dropped\r\n", "rx", (unsigned long)iface->stat.rx_frames,
           (unsigned long)iface->stat.rx_dropped);
    printf(FIELD "%lu posted, %lu pushed, %lu dropped\r\n", "tx", (unsigned long)iface->stat.tx_posted,
           (unsigned long)iface->stat.tx_pushed, (unsigned long)iface->stat.tx_dropped);
}

// What belongs to no single interface: one MAC block sits behind both, and a
// frame naming a vif nobody carries has nowhere else to go.
static void print_port(void) {
    printf("port\r\n");
    printf(FIELD "%lu\r\n", "mac resets", (unsigned long)wifi_mac_resets());
    printf(FIELD "%lu in, %lu out\r\n", "unknown vif", (unsigned long)wifi_net_rx_unknown_vif(),
           (unsigned long)wifi_net_tx_unknown_vif());
}

static void print_iface_ip(const struct net_if *iface) {
    printf("%s\r\n", iface->name);

    if (!iface->started) {
        printf(FIELD "%s\r\n", "state", "not brought up");
        return;
    }

    const ip4_addr_t *ip = netif_ip4_addr(&iface->netif);
    if (ip4_addr_isany_val(*ip)) {
        printf(FIELD "%s\r\n", "ip", "none yet");
        return;
    }

    print_ipv4("ip", ip);
    print_ipv4("netmask", netif_ip4_netmask(&iface->netif));
    print_ipv4("gateway", netif_ip4_gw(&iface->netif));
    printf(FIELD "%s\r\n", "from", dhcp_supplied_address(&iface->netif) ? "DHCP" : "set by hand");
}

// ---- Commands --------------------------------------------------------------

static void print_usage(const char *command, const char *name) {
    printf("usage: %s %s [iface]\r\n", command, name);
    printf("  iface   %s or %s; both are reported when it is left out\r\n", WIFI_IFACE_STA, WIFI_IFACE_AP);
}

// The one interface named, or every one there is.
static int report(const char *command, int argc, const char *argv[], void (*print)(const struct net_if *)) {
    if (argc >= 2 && strcmp(argv[1], "--help") == 0) {
        print_usage(command, argv[0]);
        return 0;
    }

    printf("\r\n");

    if (argc >= 2) {
        const struct net_if *iface = wifi_net_iface(argv[1]);
        if (!iface) {
            printf("[wifi] unknown interface: %s\r\n", argv[1]);
            print_usage(command, argv[0]);
            return -1;
        }
        print(iface);
        return 0;
    }

    for (size_t i = 0; i < count_of(iface_names); i++) {
        if (i) printf("\r\n");
        print(wifi_net_iface(iface_names[i]));
    }
    return 0;
}

int wifi_sub_stat(const char *command, int argc, const char *argv[]) {
    if (report(command, argc, argv, print_iface_stat) != 0) return -1;

    printf("\r\n");
    print_port();
    printf("\r\n");
    return 0;
}

int wifi_sub_ip(const char *command, int argc, const char *argv[]) {
    if (report(command, argc, argv, print_iface_ip) != 0) return -1;

    printf("\r\n");
    return 0;
}
