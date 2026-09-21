// Host→LMAC message builders. Each rw_msg_send_*() hands its ke_msg to
// rw_msg_send(), which pushes it onto the bmsg IOCTL queue and, when a
// confirmation id is given, parks a node on rw_msg_tx_head and blocks until
// rwnx_recv_msg() matches the reply.
//
// Task ids live in libip/ke.h and message ids in libip/tasks/, both read from
// libip_7221u.a's own DWARF debug info.

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <FreeRTOS.h> // pvPortMalloc / vPortFree
#include "platform/cpu.h"
#include "rtos.h"

#include "libip/message.h"
#include "libip/phy.h"
#include "rwnx_intf.h"
#include "wifi/bmsg.h"
#include "wifi/core.h"
#include "wifi/rw_msg.h"

// #define DEBUG_NAME "rw_msg"
#include "debug.h"

// ---- rw_msg_send ----------------------------------------------------------
// cfm_id == 0 sends fire-and-forget. Otherwise the caller blocks until that
// confirmation comes back, and rwnx_recv_msg() copies at most cfm_size bytes of
// it into cfm (which may be NULL for confirmations that carry no payload).
#define RW_MSG_NO_CFM         0
#define RW_MSG_CFM_TIMEOUT_MS 5000

// Ownership of a ke_msg passes to the LMAC only once the message is actually
// queued.
int rw_msg_post(void *msg_params) {
    if (bmsg_ioctl_sender(msg_params) == 0) return 0;

    ke_msg_free_by_param(msg_params);
    return -1;
}

static int rw_msg_send(void *msg_params, uint16_t cfm_id, void *cfm, uint16_t cfm_size) {
    if (cfm_id == RW_MSG_NO_CFM) return rw_msg_post(msg_params);

    struct ke_msg  *msg  = ke_param2msg(msg_params);
    msg_snd_node_t *node = pvPortMalloc(sizeof(msg_snd_node_t));
    if (!node) {
        ke_msg_free(msg);
        LOG_W("%s(cfm_id=0x%04x): out of memory", __func__, cfm_id);
        return -1;
    }

    node->cfm       = cfm;
    node->cfm_size  = cfm_size;
    node->semaphore = NULL;
    node->reqid     = cfm_id;

    if (rtos_init_semaphore(&node->semaphore, 1) != 0) {
        vPortFree(node);
        ke_msg_free(msg);
        LOG_W("%s(cfm_id=0x%04x): semaphore init failed", __func__, cfm_id);
        return -1;
    }

    // Parked before sending, so the confirmation cannot arrive unmatched.
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    co_list_push_back(&rw_msg_tx_head, &node->hdr);
    GLOBAL_INT_RESTORE();

    // A failed send leaves nothing to wait for, so undo the parking rather than
    // block for the full timeout.
    if (bmsg_ioctl_sender(msg_params) != 0) {
        GLOBAL_INT_DISABLE();
        co_list_extract(&rw_msg_tx_head, &node->hdr);
        GLOBAL_INT_RESTORE();
        rtos_deinit_semaphore(&node->semaphore);
        vPortFree(node);
        ke_msg_free(msg);
        LOG_W("%s(cfm_id=0x%04x): request could not be queued", __func__, cfm_id);
        return -1;
    }

    int ret = rtos_get_semaphore(&node->semaphore, RW_MSG_CFM_TIMEOUT_MS);
    if (ret != 0) {
        LOG_W("%s(cfm_id=0x%04x): no confirmation in %u ms", __func__, cfm_id, RW_MSG_CFM_TIMEOUT_MS);
        GLOBAL_INT_DISABLE();
        co_list_extract(&rw_msg_tx_head, &node->hdr);
        GLOBAL_INT_RESTORE();
    }
    rtos_deinit_semaphore(&node->semaphore);
    vPortFree(node);
    return ret;
}

// ---- rw_msg_send_reset ----------------------------------------------------
// Reset the LMAC. MM_RESET_CFM carries no payload, but is waited for so a failed
// reset stops the bring-up.
int rw_msg_send_reset(void) {
    void *req = ke_msg_alloc(MM_RESET_REQ, TASK_MM, TASK_API, 0);
    if (!req) return -1;

    LOG_I("MM_RESET_REQ");
    return rw_msg_send(req, MM_RESET_CFM, NULL, 0);
}

// ---- rw_msg_send_me_config_req --------------------------------------------
// Advertise host capabilities to the ME task. Values copied from the reference
// build's RWNX_HT_CAPABILITIES (func/rwnx_intf/rw_ieee80211.h).
//
// Claiming 802.11n drives rc_basic_init(), which picks the station's whole rate
// policy from it: without HT it programs FORMATMOD_NON_HT rate index 0 (1 Mbps
// CCK), which an OFDM-only AP never completes; with HT it programs MCS0.
int rw_msg_send_me_config_req(void) {
    struct me_config_req *req = ke_msg_alloc(ME_CONFIG_REQ, TASK_ME, TASK_API, sizeof(*req));
    if (!req) return -1;

    // 0x01AC: SM power save disabled, short GI at 20 MHz, TX STBC, RX STBC on
    // one stream. 20 MHz only — bit 1 (channel width) stays clear.
    req->ht_cap.ht_capa_info = 0x01AC;
    // Max A-MPDU exponent 0 (8 KB) with minimum spacing 7 (16 us), which sits in
    // bits 4:2.
    req->ht_cap.a_mpdu_param = 0 | (7u << 2);

    // struct ieee80211_mcs_info laid out flat: rx_mask[10], rx_highest(2),
    // tx_params(1), 3 reserved.
    req->ht_cap.mcs_rate[0]  = 0xFF; // MCS 0-7, one spatial stream
    req->ht_cap.mcs_rate[10] = 0x00; // rx_highest, little endian
    req->ht_cap.mcs_rate[11] = 0x01;
    req->ht_cap.mcs_rate[12] = 0x01; // tx_params: the TX MCS set is defined

    req->ht_cap.ht_extended_capa    = 0;
    req->ht_cap.tx_beamforming_capa = 0x64000000;
    req->ht_cap.asel_capa           = 0x01;

    req->ht_supp = true;
    // vht_supp stays false — this radio is 2.4 GHz only.
    // ps_on stays false, matching the reference build.
    // tx_lft stays 0: it only bounds host data frames queued behind a BlockAck
    // agreement (bam.c), and nothing here sends those yet.

    LOG_I("ME_CONFIG_REQ ht=1 capa=0x%04x", req->ht_cap.ht_capa_info);
    return rw_msg_send(req, ME_CONFIG_CFM, NULL, 0);
}

// ---- rw_msg_send_me_chan_config_req ---------------------------------------
// Tell the ME task which channels are available (14 x 2.4 GHz).
int rw_msg_send_me_chan_config_req(void) {
    struct me_chan_config_req *req = ke_msg_alloc(ME_CHAN_CONFIG_REQ, TASK_ME, TASK_API, sizeof(*req));
    if (!req) return -1;

    for (int i = 0; i < SCAN_CHANNEL_2G4; i++) {
        req->chan2G4[i].freq     = rw_ieee80211_get_centre_frequency((uint32_t)i + 1u);
        req->chan2G4[i].band     = 0; // 2.4 GHz
        req->chan2G4[i].flags    = 0;
        req->chan2G4[i].tx_power = 20;
    }
    req->chan2G4_cnt = SCAN_CHANNEL_2G4;
    req->chan5G_cnt  = 0;
    LOG_I("ME_CHAN_CONFIG_REQ chan2G4=%u", req->chan2G4_cnt);
    return rw_msg_send(req, ME_CHAN_CONFIG_CFM, NULL, 0);
}

// ---- rw_msg_send_start ----------------------------------------------------
// Start the MAC firmware. This is what makes the LMAC call phy_init().
int rw_msg_send_start(void) {
    struct mm_start_req *req = ke_msg_alloc(MM_START_REQ, TASK_MM, TASK_API, sizeof(*req));
    if (!req) return -1;

    // parameters[0] is phy_trd_cfg_tag.path_mapping, [1] its tx_dc_off_comp —
    // byte-for-byte what the vendor sends.
    req->phy_cfg_parameters[0] = 1;
    req->phy_cfg_parameters[1] = 0;
    req->uapsd_timeout         = 300;
    req->lp_clk_accuracy       = 20;

    LOG_I("MM_START_REQ");
    return rw_msg_send(req, MM_START_CFM, NULL, 0);
}

// ---- rw_msg_send_add_if ---------------------------------------------------
// Add a virtual interface. cfm->inst_nbr receives the LMAC-assigned VIF index.
int rw_msg_send_add_if(uint8_t type, const uint8_t mac[6], struct mm_add_if_cfm *cfm) {
    struct mm_add_if_req *req = ke_msg_alloc(MM_ADD_IF_REQ, TASK_MM, TASK_API, sizeof(*req));
    if (!req) return -1;

    req->type = type;
    memcpy(&req->addr, mac, 6);
    req->p2p = false;

    LOG_I("MM_ADD_IF_REQ type=%u", type);
    return rw_msg_send(req, MM_ADD_IF_CFM, cfm, sizeof(*cfm));
}

// ---- rw_msg_set_channel ----------------------------------------------------
// Sends MM_SET_CHANNEL_REQ. Keeps the vendor name because libip(rwnx.o) imports
// it, and takes three arguments, not the two the SDK sources show.
//
// MM_SET_CHANNEL_CFM arrives only after phy_set_channel() has returned, so the
// radio is on the new channel by the time this does. In MM_HOST_BYPASSED,
// mm_hw_config_handler short-circuits the message and never tunes at all.
int rw_msg_set_channel(uint32_t channel, uint32_t band_width, __unused void *cfm) {

    uint16_t freq = rw_ieee80211_get_centre_frequency(channel);
    if (!freq) {
        LOG_W("%s(channel=%u): not a 2.4 GHz channel", __func__, (unsigned)channel);
        return -1;
    }

    struct mm_set_channel_req *req = ke_msg_alloc(MM_SET_CHANNEL_REQ, TASK_MM, TASK_API, sizeof(*req));
    if (!req) return -1;

    req->band         = PHY_BAND_2G4;
    req->type         = band_width;
    req->prim20_freq  = freq;
    req->center1_freq = freq;
    req->center2_freq = 0;
    req->index        = PHY_SEC;
    req->tx_power     = 0;

    LOG_I("MM_SET_CHANNEL_REQ channel=%u freq=%u", (unsigned)channel, freq);

    // Two bytes wide and left uninitialised: the channel-context branch of the
    // handler allocates the confirmation and sends it without writing either
    // field. Waited for, not read.
    return rw_msg_send(req, MM_SET_CHANNEL_CFM, NULL, 0);
}

// ---- rw_msg_send_sm_connect_req -------------------------------------------
// Request an open (no WPA) association. vif_idx comes from
// rw_msg_send_add_if(); bssid and channel say which BSS to join.
//
// Blocks briefly for SM_CONNECT_CFM, which reports only whether the request was
// accepted. The association result itself arrives later as SM_CONNECT_IND.
int rw_msg_send_sm_connect_req(uint8_t vif_idx, const char *ssid, const uint8_t bssid[6], uint8_t channel) {
    struct sm_connect_req *req;
    size_t ssid_len = strlen(ssid);
    if (ssid_len > sizeof(req->ssid.array)) return -1;

    req = ke_msg_alloc(SM_CONNECT_REQ, TASK_SM, TASK_API, sizeof(*req));
    if (!req) return -1;

    // The reference build announces the attempt to TASK_SM here; the indication
    // only sets the host's own station status, which connect.cpp does directly:
    // ke_msg_send_basic(SM_CONNECTION_START_IND, TASK_API, TASK_SM);

    req->ssid.length = (uint8_t)ssid_len;
    memcpy(req->ssid.array, ssid, ssid_len);
    // sm_get_bss_params() reads the group bit of the first byte, and a set bit
    // means "no BSSID given, look the SSID up in the scan cache instead", which
    // makes FF:FF:FF:FF:FF:FF the only way to ask for that.
    memcpy(&req->bssid, bssid, sizeof(req->bssid));

    req->vif_idx   = vif_idx;
    req->chan.freq = rw_ieee80211_get_centre_frequency(channel);
    req->chan.band = 0;
    // Keeps the vendor's name, but on this chip the bit makes nothing passive:
    // it only buys 110 ms of channel time instead of 90 (libip/tasks/scan.h),
    // the window sm_join_bss() has to hear the probe response in.
    req->chan.flags = SCAN_PASSIVE_BIT;

    // Zero is what an open network wants. The bits (mac.h) are
    // CONTROL_PORT_HOST, CONTROL_PORT_NO_ENC, DISABLE_HT, WPA_WPA2_IN_USE and
    // MFP_IN_USE, none of which mean anything before WPA.
    req->flags           = 0;
    req->listen_interval = 100;
    req->auth_type       = 0; // open

    struct sm_connect_cfm cfm = {0};
    LOG_I("SM_CONNECT_REQ ssid=\"%s\" vif=%u", ssid, vif_idx);
    int ret = rw_msg_send(req, SM_CONNECT_CFM, &cfm, sizeof(cfm));
    if (ret != 0) return ret;

    LOG_I("SM_CONNECT_CFM status=%u", cfm.status);
    // Non-zero means the request was rejected outright, so no SM_CONNECT_IND
    // follows.
    return cfm.status == 0 ? 0 : -1;
}

// ---- rw_msg_send_sm_disconnect_req -----------------------------------------
// Leave the BSS. The confirmation says only that SM acted, which it does once
// it is idle; the deauthentication itself and the SM_DISCONNECT_IND that
// reports it both follow.
int rw_msg_send_sm_disconnect_req(uint8_t vif_idx, uint16_t reason_code) {
    struct sm_disconnect_req *req = ke_msg_alloc(SM_DISCONNECT_REQ, TASK_SM, TASK_API, sizeof(*req));
    if (!req) return -1;

    req->reason_code = reason_code;
    req->vif_idx     = vif_idx;

    LOG_I("SM_DISCONNECT_REQ vif=%u reason=%u", vif_idx, reason_code);
    return rw_msg_send(req, SM_DISCONNECT_CFM, NULL, 0);
}
