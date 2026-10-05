#include <stdint.h>
#include <stddef.h>

// #define DEBUG_NAME "misc"
#include "debug.h"

// ---- LMAC → supplicant management frame pipe (func/hostapd-2.5/bk_patch/sk_intf.c) ----
// imported: libip(me_task.o, rxu_cntrl.o)
// In the SDK this delivers 802.11 management frames to wpa_supplicant/hostapd
// via an internal "kernel socket" (ke_sk_send). No supplicant is linked here, so
// the frame is dropped and success reported: a non-zero return would send the
// LMAC into wpa_hostapd_queue_poll(), which is equally absent.
int ke_mgmt_packet_tx(unsigned char *buf, int len, int flag) {
    LOG_W("%s(len=%d, vif=%d): dropped (no supplicant)", __func__, len, flag);
    (void)buf;
    return 0;
}

// ---- RSN / WPA IE parsing ---------------------------------------------------
// imported: libip(rxu_cntrl.o)
// Open networks only for now: reporting "no WPA IE" makes the LMAC treat every
// BSS as unencrypted. scan.cpp does its own RSN parsing for display, so
// the scan list still shows real security types.
int wpa_parse_wpa_ie(const uint8_t *wpa_ie, uint32_t wpa_ie_len, void *data) {
    (void)wpa_ie;
    (void)wpa_ie_len;
    (void)data;
    return -1;
}

// ---- Role-launch cancel hooks (func/joint_up/role_launch.c) -----------------
// imported: libip(mm_bcn.o)
// The vendor's role_launch state machine lets the app abort a STA/AP bring-up
// already in flight: the LMAC calls these before a beacon transmit or a channel
// switch and gives up when the returned cancel flag is set. Nothing here ever
// cancels, so the answer is always "keep going".

uint32_t rl_pre_sta_set_status(uint32_t status) {
    (void)status;
    return 0;
}

uint32_t rl_pre_ap_set_status(uint32_t status) {
    (void)status;
    return 0;
}

// Real version also calls mm_hw_ap_disable() when the AP launch was canceled.
uint32_t rl_pre_ap_disable_autobcn(void) { return 0; }

// ---- TX EVM test hooks (func/rf_test/tx_evm.c) ------------------------------
// imported: libip(ke_event.o, txl_cntrl.o)
// evm_via_mac_evt sits in the ke_evt_hdlr table as the KE_EVT_EVM_MAC handler;
// evm_via_mac_continue runs on every TX trigger and re-arms that event. Both do
// nothing until the EVM transmit test is started, which this port never does.

void evm_via_mac_evt(int dummy) { (void)dummy; }

void evm_via_mac_continue(void) {}
