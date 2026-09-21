// Bringing an interface up: one LMAC vif and one lwIP netif per entry in the
// table, created the first time something asks for that interface by name.
//
// The LMAC's own configuration — reset, me_config, me_chan_config, start — is
// global rather than per-interface, and has to precede the first add_if.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lwip/dhcp.h"
#include "lwip/netifapi.h"
#include "lwip/tcpip.h"

#include "wifi/core.h"
#include "wifi/net.h"
#include "wifi/rw_msg.h"
#include "wifi/station_status.h"

#include "iface.h"

// #define DEBUG_NAME "wifi_iface"
#include "debug.h"

#undef count_of
#define count_of(x) (sizeof(x) / sizeof(x[0]))

// Beken's own OUI, and a last byte that keeps the two apart: one radio cannot
// carry the same address twice. netif_add() leaves hwaddr alone, so what is set
// here is what lwIP ends up with; wifi_net_set_mac() replaces either.
static struct net_if wifi_ifaces[IFACE_COUNT] = {
    [IFACE_STA] =
        {
            .name  = WIFI_IFACE_STA,
            .type  = VIF_STA,
            .vif   = INVALID_VIF_IDX,
            .netif = {.hwaddr = {0xC8, 0x47, 0x8C, 0x42, 0x88, 0x48}, .hwaddr_len = NETIF_MAX_HWADDR_LEN},
        },
    [IFACE_AP] =
        {
            .name  = WIFI_IFACE_AP,
            .type  = VIF_AP,
            .vif   = INVALID_VIF_IDX,
            .netif = {.hwaddr = {0xC8, 0x47, 0x8C, 0x42, 0x88, 0x49}, .hwaddr_len = NETIF_MAX_HWADDR_LEN},
        },
};

struct net_if *wifi_net_iface(const char *name) {
    for (size_t i = 0; i < count_of(wifi_ifaces); i++) {
        if (strcmp(name, wifi_ifaces[i].name) == 0) return &wifi_ifaces[i];
    }
    return NULL;
}

struct net_if *iface_from_vif(uint8_t vif_idx) {
    if (vif_idx == INVALID_VIF_IDX) return NULL;

    for (size_t i = 0; i < count_of(wifi_ifaces); i++) {
        if (wifi_ifaces[i].started && wifi_ifaces[i].vif == vif_idx) return &wifi_ifaces[i];
    }
    return NULL;
}

bool wifi_net_set_mac(const char *name, const uint8_t mac[NETIF_MAX_HWADDR_LEN]) {
    struct net_if *iface = wifi_net_iface(name);
    if (!iface) {
        LOG_E("%s: no interface called %s", __func__, name);
        return false;
    }

    // The address goes to the LMAC inside MM_ADD_IF_REQ and to lwIP at
    // netif_add(), so a later one would not reach either.
    if (iface->started) {
        LOG_W("%s: %s is already up", __func__, iface->name);
        return false;
    }

    if (mac[0] & 0x01) {
        LOG_W("%s: %02x:%02x:%02x:%02x:%02x:%02x is multicast, ignoring", __func__, mac[0], mac[1], mac[2], mac[3],
              mac[4], mac[5]);
        return false;
    }

    const uint8_t zero[NETIF_MAX_HWADDR_LEN] = {0};
    if (memcmp(mac, zero, sizeof(zero)) == 0) {
        LOG_W("%s: all-zero address, ignoring", __func__);
        return false;
    }

    memcpy(iface->netif.hwaddr, mac, NETIF_MAX_HWADDR_LEN);
    iface->netif.hwaddr_len = NETIF_MAX_HWADDR_LEN;
    LOG_I("%s: %s is %02x:%02x:%02x:%02x:%02x:%02x", __func__, iface->name, mac[0], mac[1], mac[2], mac[3], mac[4],
          mac[5]);
    return true;
}

// ---- Bring-up --------------------------------------------------------------

// In this order, and the LMAC refuses everything later if one is skipped.
static const struct {
    int (*send)(void);
    const char *name;
} configure_steps[] = {
    {rw_msg_send_reset, "reset"},
    {rw_msg_send_me_config_req, "me_config"},
    {rw_msg_send_me_chan_config_req, "me_chan_config"},
    {rw_msg_send_start, "start"},
};

// What the vendor gates this on too (sa_station.c, sa_ap.c): an interface only
// reaches vif_mgmt_env's used list through add_if, which the steps above
// precede, so an empty list means none of them has run. Asking the archive
// rather than keeping a flag also retries a sequence that failed part way.
static bool configure_lmac(void) {
    if (!co_list_is_empty(&vif_mgmt_env.used_list)) return true;

    for (size_t i = 0; i < count_of(configure_steps); i++) {
        if (configure_steps[i].send() != 0) {
            LOG_E("%s: the LMAC refused %s", __func__, configure_steps[i].name);
            return false;
        }
    }

    return true;
}

static bool register_vif(struct net_if *iface) {
    struct mm_add_if_cfm cfm = {.status = 0xFF, .inst_nbr = INVALID_VIF_IDX};

    if (rw_msg_send_add_if(iface->type, iface->netif.hwaddr, &cfm) != 0) {
        LOG_E("%s: no reply to add_if for %s", __func__, iface->name);
        return false;
    }

    if (cfm.status != 0) {
        LOG_E("%s: add_if refused %s, status=%u", __func__, iface->name, cfm.status);
        return false;
    }

    // Checked here because this is where the index enters: the transmit path
    // uses it to subscript the archive's own interface table.
    if (cfm.inst_nbr >= NX_VIRT_DEV_MAX) {
        LOG_E("%s: add_if gave %s vif=%u, past the LMAC's table", __func__, iface->name, cfm.inst_nbr);
        return false;
    }

    iface->vif = cfm.inst_nbr;
    return true;
}

// Runs on the tcpip thread. mm.c gates the archive's beacon-loss handling on
// MSG_GOT_IP, so a DHCP lease has to be reported here.
static void status_changed(struct netif *nif) {
    const ip4_addr_t *ip = netif_ip4_addr(nif);

    if (ip4_addr_isany_val(*ip)) {
        if (mhdr_get_station_status() == MSG_GOT_IP) mhdr_set_station_status(MSG_CONN_SUCCESS);
        return;
    }

    mhdr_set_station_status(MSG_GOT_IP);
    printf("got address %u.%u.%u.%u\r\n", ip4_addr1(ip), ip4_addr2(ip), ip4_addr3(ip), ip4_addr4(ip));
}

static bool netif_create(struct net_if *iface) {
    // The netifapi_* calls take the stack's lock themselves, which the plain
    // ones do not — and none of this runs on the tcpip thread.
    if (netifapi_netif_add(&iface->netif, IP4_ADDR_ANY4, IP4_ADDR_ANY4, IP4_ADDR_ANY4, NULL, ethernetif_init,
                           tcpip_input) != ERR_OK) {
        LOG_E("%s: lwIP would not create %s", __func__, iface->name);
        return false;
    }

    netifapi_netif_set_up(&iface->netif);

    // The default route and the station status both belong to a station; neither
    // has an AP meaning.
    if (iface->type == VIF_STA) {
        netifapi_netif_set_default(&iface->netif);

        LOCK_TCPIP_CORE();
        netif_set_status_callback(&iface->netif, status_changed);
        UNLOCK_TCPIP_CORE();
    }

    return true;
}

struct net_if *wifi_net_ensure_iface(const char *name) {
    struct net_if *iface = wifi_net_iface(name);
    if (!iface) {
        LOG_E("%s: no interface called %s", __func__, name);
        return NULL;
    }

    if (iface->started) return iface;

    // Every step below waits for a confirmation only the core task delivers.
    if (!wifi_core_running()) {
        LOG_E("%s: the WiFi core task is not running", __func__);
        return NULL;
    }

    if (!configure_lmac()) return NULL;
    if (!register_vif(iface)) return NULL;
    if (!netif_create(iface)) return NULL;

    iface->started = true;

    __unused const uint8_t *mac = iface->netif.hwaddr;
    LOG_I("%s up on vif=%u, %02x:%02x:%02x:%02x:%02x:%02x", iface->name, iface->vif, mac[0], mac[1], mac[2], mac[3],
          mac[4], mac[5]);

    return iface;
}

// ---- Link and addresses ----------------------------------------------------

void wifi_net_link_up(uint8_t vif_idx) {
    struct net_if *iface = &wifi_ifaces[IFACE_STA];
    if (!iface->started) return;

    iface->vif = vif_idx;

    netifapi_netif_set_link_up(&iface->netif);
    if (netifapi_dhcp_start(&iface->netif) != ERR_OK) {
        LOG_W("%s: DHCP could not be started", __func__);
        return;
    }
    LOG_I("link up on vif=%u, asking for an address", vif_idx);
}

void wifi_net_link_down(void) {
    struct net_if *iface = &wifi_ifaces[IFACE_STA];
    if (!iface->started) return;

    netifapi_dhcp_stop(&iface->netif);
    netifapi_netif_set_link_down(&iface->netif);
    LOG_I("link down");
}

bool wifi_net_get_address(uint32_t *addr, uint32_t *netmask, uint32_t *gateway) {
    struct net_if *iface = &wifi_ifaces[IFACE_STA];
    if (!iface->started || ip4_addr_isany_val(*netif_ip4_addr(&iface->netif))) return false;

    if (addr) *addr = lwip_ntohl(ip4_addr_get_u32(netif_ip4_addr(&iface->netif)));
    if (netmask) *netmask = lwip_ntohl(ip4_addr_get_u32(netif_ip4_netmask(&iface->netif)));
    if (gateway) *gateway = lwip_ntohl(ip4_addr_get_u32(netif_ip4_gw(&iface->netif)));
    return true;
}

void wifi_net_set_address(uint32_t addr, uint32_t netmask, uint32_t gateway) {
    struct net_if *iface = &wifi_ifaces[IFACE_STA];
    if (!iface->started) return;

    if (!addr) {
        netifapi_netif_set_addr(&iface->netif, IP4_ADDR_ANY4, IP4_ADDR_ANY4, IP4_ADDR_ANY4);
        if (netifapi_dhcp_start(&iface->netif) != ERR_OK) LOG_W("%s: DHCP could not be started", __func__);
        return;
    }

    // Stopped first: DHCP would otherwise overwrite what we just set the moment
    // an offer arrived.
    netifapi_dhcp_stop(&iface->netif);

    ip4_addr_t a, n, g;
    ip4_addr_set_u32(&a, lwip_htonl(addr));
    ip4_addr_set_u32(&n, lwip_htonl(netmask));
    ip4_addr_set_u32(&g, lwip_htonl(gateway));
    netifapi_netif_set_addr(&iface->netif, &a, &n, &g);
}
