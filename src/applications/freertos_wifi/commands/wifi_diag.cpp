// // wifi_diag — one screen that says where the link actually is.
// //
// // The first block is the link itself, because every other question starts with
// // "joined what, on which channel, how loud". Three of its lines live inside the
// // vendor archive and are read here directly; see vif_info_probe.
//
// #include <stdint.h>
// #include <stdio.h>
// #include <string.h>
//
// #include <FreeRTOS.h>
// #include <queue.h>
// #include <task.h>
//
// #include "libip/ke.h"
// #include "libip/phy.h"
// #include "libip/tasks/mm.h"
// #include "libip/tasks/sm.h"
// #include "libip/vif.h"
// #include "wifi/core.h"
// #include "wifi/net.h"
// #include "wifi/rw_msg.h"
// #include "wifi/station_status.h"
//
//
// #undef count_of
// #define count_of(x) (sizeof(x) / sizeof(x[0]))
//
//
// struct id_name_map {
//     uint32_t    id;
//     const char *name;
// };
//
// static const char *get_name_by_id(const struct id_name_map *map, const uint32_t id, const size_t count) {
//     for (size_t i = 0; i < count; i++) {
//         if (map[i].id == id) return map[i].name;
//     }
//
//     return "unknown";
// }
//
// #define GET_NAME_LABEL(value, map) get_name_by_id(map, value, count_of(map))
//
// // clang-format off
// static const struct id_name_map status_name_labels[] = {
//     {MSG_IDLE, "IDLE"},
//     {MSG_CONNECTING, "CONNECTING"},
//     {MSG_PASSWD_WRONG, "PASSWD_WRONG"},
//     {MSG_NO_AP_FOUND, "NO_AP_FOUND"},
//     {MSG_CONN_FAIL, "CONN_FAIL"},
//     {MSG_CONN_SUCCESS, "CONN_SUCCESS"},
//     {MSG_GOT_IP, "GOT_IP"},
// };
//
// static const struct id_name_map sm_state_labels[] = {
//     {SM_IDLE, "IDLE"},
//     {SM_SCANNING, "SCANNING"},
//     {SM_JOINING, "JOINING"},
//     {SM_STA_ADDING, "STA_ADDING"},
//     {SM_DISABLING_PS, "DISABLING_PS"},
//     {SM_BSS_PARAM_SETTING, "BSS_PARAM_SETTING"},
//     {SM_AUTHENTICATING, "AUTHENTICATING"},
//     {SM_ASSOCIATING, "ASSOCIATING"},
//     {SM_ACTIVATING, "ACTIVATING"},
//     {SM_DISCONNECTING, "DISCONNECTING"},
// };
//
// static const struct id_name_map mm_state_labels[] = {
//     {MM_IDLE, "IDLE"},
//     {MM_ACTIVE, "ACTIVE"},
//     {MM_GOING_TO_IDLE, "GOING_TO_IDLE"},
//     {MM_HOST_BYPASSED, "HOST_BYPASSED"},
// };
//
// static const struct id_name_map bandwidth_labels[] = {
//     {PHY_CHNL_BW_20, "20 MHz"},
//     {PHY_CHNL_BW_40, "40 MHz"},
//     {PHY_CHNL_BW_80, "80 MHz"},
//     {PHY_CHNL_BW_160, "160 MHz"},
//     {PHY_CHNL_BW_80P80, "80+80 MHz"},
// };
//
// // Lowercase because these name a block heading, not a state.
// static const struct id_name_map vif_type_labels[] = {
//     {VIF_STA, "sta"},
//     {VIF_IBSS, "ibss"},
//     {VIF_AP, "ap"},
//     {VIF_MESH_POINT, "mesh point"},
// };
// // clang-format on
//
// #define GET_STATUS_NAME(value)    GET_NAME_LABEL(value, status_name_labels)
// #define GET_SM_STATE_NAME(value)  GET_NAME_LABEL(value, sm_state_labels)
// #define GET_MM_STATE_NAME(value)  GET_NAME_LABEL(value, mm_state_labels)
// #define GET_BANDWIDTH_NAME(value) GET_NAME_LABEL(value, bandwidth_labels)
// #define GET_VIF_TYPE_NAME(value)  GET_NAME_LABEL(value, vif_type_labels)
//
//
// // ---- Archive internals -----------------------------------------------------
//
// // The station interface, or nullptr when there is none. Everything the archive
// // records about a live link — signal, missed beacons, failed probes — hangs off
// // this one entry, so several blocks below need it.
// static const struct vif_info_tag *find_sta_vif(void) {
//     for (unsigned i = 0; i < NX_VIRT_DEV_MAX; i++) {
//         if (vif_info_tab[i].active && vif_info_tab[i].type == VIF_STA) return &vif_info_tab[i];
//     }
//     return nullptr;
// }
//
//
// // ---- Printing --------------------------------------------------------------
//
// #define FIELD "  %-13s"
//
// static void print_mac(const char *label, const struct mac_addr *addr) {
//     const uint8_t *b = reinterpret_cast<const uint8_t *>(addr->array);
//     printf(FIELD "%02x:%02x:%02x:%02x:%02x:%02x\r\n", label, b[0], b[1], b[2], b[3], b[4], b[5]);
// }
//
// static void print_ipv4(const char *label, uint32_t addr) {
//     printf(FIELD "%lu.%lu.%lu.%lu\r\n", label, (addr >> 24) & 0xFFu, (addr >> 16) & 0xFFu, (addr >> 8) & 0xFFu,
//            addr & 0xFFu);
// }
//
// // Our own view of the link, then the archive's. They are kept apart because they
// // can disagree: a MAC reset drops the LMAC back to idle without telling us, and
// // the first line then still claims success.
// static void print_wifi(void) {
//     printf("Wi-Fi\r\n");
//     printf(FIELD "%s\r\n", "state", GET_STATUS_NAME(mhdr_get_station_status()));
//     printf(FIELD "%s\r\n", "sm task", GET_SM_STATE_NAME(ke_state_get(TASK_SM)));
//     printf(FIELD "%s\r\n", "mm task", GET_MM_STATE_NAME(ke_state_get(TASK_MM)));
// }
//
// static void print_network(void) {
//     printf("Network\r\n");
//
//     uint32_t addr = 0, netmask = 0, gateway = 0;
//     if (!wifi_net_get_address(&addr, &netmask, &gateway)) {
//         printf(FIELD "%s\r\n", "ip", "none, DHCP has not finished");
//         return;
//     }
//
//     print_ipv4("ip", addr);
//     print_ipv4("netmask", netmask);
//     print_ipv4("gateway", gateway);
// }
//
// // Where the radio actually sits, which need not be the BSS's channel: a scan
// // leaves it wherever the sweep ended.
// static void print_radio(const struct vif_info_tag *sta) {
//     printf("Radio\r\n");
//
//     struct phy_channel_info chan = {};
//     phy_get_channel(&chan, PHY_PRIM);
//     uint32_t freq = (chan.info1 >> 16) & 0xFFFFu;
//     uint32_t type = (chan.info1 >> 8) & 0xFFu;
//     uint8_t  ch   = rw_ieee80211_get_chan_id(freq);
//
//     if (ch) {
//         printf(FIELD "%u (%lu MHz)\r\n", "channel", (unsigned)ch, freq);
//         printf(FIELD "%s\r\n", "bandwidth", GET_BANDWIDTH_NAME(type));
//     } else {
//         printf(FIELD "%s\r\n", "channel", "not tuned yet");
//     }
//
//     if (sta) printf(FIELD "%d dBm\r\n", "rssi", sta->u.sta.rssi);
// }
//
// // One block per interface the LMAC has registered. There are two slots and both
// // can be live at once, so this loops rather than reporting the first hit.
// static void print_interfaces(void) {
//     bool any = false;
//
//     for (unsigned i = 0; i < NX_VIRT_DEV_MAX; i++) {
//         const struct vif_info_tag *vif = &vif_info_tab[i];
//         if (!vif->active) continue;
//
//         any = true;
//         printf("\r\nInterface %s\r\n", GET_VIF_TYPE_NAME(vif->type));
//         print_mac("mac", &vif->mac_addr);
//         print_mac("bssid", &vif->bssid);
//     }
//
//     if (!any) printf("\r\nInterface none\r\n");
// }
//
// static void print_stats(const struct vif_info_tag *sta) {
//     printf("Stats\r\n");
//
//     if (sta) {
//         printf(FIELD "%u\r\n", "beacon loss", (unsigned)sta->u.sta.beacon_loss_cnt);
//         printf(FIELD "%u\r\n", "probe fail", (unsigned)sta->u.sta.mm_retry);
//     }
//
//     printf(FIELD "%lu\r\n", "resets", wifi_mac_resets());
//
//     struct wifi_net_counters c = {};
//     wifi_net_counters(&c);
//     printf(FIELD "%lu frames, %lu dropped\r\n", "rx", c.rx_frames, c.rx_dropped);
//     printf(FIELD "%lu sent, %lu in flight, %lu dropped\r\n", "tx", c.tx_confirmed, c.tx_pushed - c.tx_confirmed,
//            c.tx_dropped);
// }
//
// int command_wifi_diag(__unused int argc, __unused const char *argv[]) {
//     if (!wifi_core_running()) {
//         printf("[wifi] not started, run wifi_start first\r\n");
//         return -1;
//     }
//
//     const struct vif_info_tag *sta = find_sta_vif();
//
//     printf("\r\n");
//     print_wifi();
//     printf("\r\n");
//     print_network();
//     printf("\r\n");
//     print_radio(sta);
//     print_interfaces();
//     printf("\r\n");
//     print_stats(sta);
//     printf("\r\n");
//
//     return 0;
// }
