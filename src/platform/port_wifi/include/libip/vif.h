#pragma once

// ip/lmac/src/vif/vif_mgmt.h — the virtual interfaces the LMAC keeps, and the
// table it keeps them in. One entry per interface this port asks for, and the
// only place the archive publishes what a live link looks like: whose BSS,
// what signal, how many beacons went missing.
//
// The vendor header wraps almost every field in a build-time #if, so its shape
// on paper is not the shape in the archive. This is the shape in the archive:
// sizes, offsets and field types were read out of libip_7221u.a's own DWARF and
// every one of them is asserted below.
//
// Which options that layout implies, since nothing else records them: power
// save, connection monitoring, multi-role and channel contexts are on; U-APSD,
// P2P, TDLS and mesh are off.

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "libip/co_list.h"
#include "libip/mac.h"
#include "libip/tx.h" // AC_MAX, the width of txq_params

#ifdef __cplusplus
extern "C" {
#endif

// enum vif_type. VIF_STA is the only one this port asks for.
enum {
    VIF_STA = 0,
    VIF_IBSS,
    VIF_AP,
    VIF_MESH_POINT,
    VIF_UNKNOWN,
};

// No interface, where an interface index is expected.
#define INVALID_VIF_IDX 0xFF

// tx_power carries this when no power has been worked out yet.
#define VIF_UNDEF_POWER 0x7F

// NX_VIRT_DEV_MAX. The archive's table is 1632 bytes, which is two entries.
#define NX_VIRT_DEV_MAX 2

// The STA arm of vif_info_tag's union.
//
// beacon_loss_cnt counts every TBTT that brought no beacon; once it passes the
// archive's threshold the AP is probed with a null frame and the MAC restarts.
struct vif_sta_info {
    uint16_t listen_interval; // 0
    bool     dont_wait_bcmc;  // 2
    uint8_t  ps_retry;        // 3
    uint8_t  ap_id;           // 4, the AP's slot in the station table
    uint8_t  reserved_5[3];
    uint32_t mon_last_tx;     // 8, when the last keep-alive went out
    uint32_t mon_last_crc;    // 12
    uint8_t  beacon_loss_cnt; // 16
    uint8_t  reserved_17[3];
    uint32_t mon_ie_crc;  // 20
    int8_t   rssi;        // 24
    int8_t   rssi_thold;  // 25, 0 = no threshold set
    uint8_t  rssi_hyst;   // 26
    bool     rssi_status; // 27, 1 = below the threshold
    uint8_t  csa_count;   // 28
    bool     csa_occured; // 29
    uint8_t  mm_retry;    // 30, consecutive failed AP probes
    uint8_t  reserved_31;
};

// The AP arm of the same union, and the larger of the two — it is what sets the
// union's 124 bytes. Nothing in this port fills it in: no AP mode.
//
// bcn_desc is the descriptor the hardware transmits beacons from on its own,
// without the host being asked each time. Everything from tim_len down is the
// traffic indication map inside that beacon, which is how an AP tells sleeping
// stations they have something waiting.
struct vif_ap_info {
    uint32_t dummy;               // 0
    uint8_t  bcn_desc[96];        // 4, struct txl_frame_desc_tag
    uint16_t bcn_len;             // 100, beacon length without the TIM element
    uint16_t tim_len;             // 102
    uint16_t tim_bitmap_set;      // 104, bits currently set in the TIM bitmap
    uint16_t bcn_int;             // 106, beacon interval in TUs
    uint8_t  bcn_tbtt_ratio;      // 108, TBTT interrupts between our beacons
    uint8_t  bcn_tbtt_cnt;        // 109, TBTT interrupts still to wait
    bool     bcn_configured;      // 110, has the host downloaded a beacon yet
    uint8_t  dtim_count;          // 111
    uint8_t  tim_n1;              // 112, byte index of the TIM bitmap's LSB
    uint8_t  tim_n2;              // 113, and of its MSB
    uint8_t  bc_mc_status;        // 114, 1 = broadcast/multicast is buffered
    uint8_t  csa_count;           // 115, channel switch announcement countdown
    uint8_t  csa_action_count;    // 116
    uint8_t  csa_oft[2];          // 117, CSA offsets within the beacon
    uint8_t  ps_sta_cnt;          // 119, connected stations that are asleep
    uint16_t ctrl_port_ethertype; // 120
    uint8_t  reserved_122[2];
};

// Fields the archive owns but this port has no type for are kept as sized
// placeholders named after what they hold, so the layout stays readable without
// dragging in the timer, channel-context, frame-descriptor and key headers.
struct vif_info_tag {
    struct co_list_hdr list_hdr;           // 0
    uint32_t           prevent_sleep;      // 4
    uint32_t           txq_params[AC_MAX]; // 8, EDCA parameters per queue
    uint8_t            tbtt_timer[20];     // 24, struct mm_timer_tag
    uint8_t            tmr_bcn_to[20];     // 44, struct mm_timer_tag
    struct mac_addr    bssid;              // 64
    uint8_t            reserved_70[2];
    void              *chan_ctxt;       // 72, struct chan_ctxt_tag *
    uint8_t            tbtt_switch[12]; // 76, struct chan_tbtt_tag
    struct mac_addr    mac_addr;        // 88
    uint8_t            type;            // 94, enum vif_type
    uint8_t            index;           // 95
    bool               active;          // 96
    int8_t             tx_power;        // 97, dBm
    int8_t             user_tx_power;   // 98
    uint8_t            reserved_99;
    union {
        struct vif_sta_info sta; // 100
        struct vif_ap_info  ap;  // and the wider arm, 124 bytes
    } u;
    struct co_list sta_list;        // 224
    uint8_t        bss_info[144];   // 232, struct mac_bss_info
    uint8_t        key_info[416];   // 376, struct key_info_tag[4]
    void          *default_key;     // 792
    uint32_t       flags;           // 796
    uint8_t        csa_channel[10]; // 800, struct mm_chan_ctxt_add_req
    uint8_t        reserved_810[2];
    void          *priv; // 812
};

static_assert(sizeof(struct vif_sta_info) == 32, "the STA union arm is 32 bytes in the archive");
static_assert(offsetof(struct vif_sta_info, ap_id) == 4, "");
static_assert(offsetof(struct vif_sta_info, beacon_loss_cnt) == 16, "");
static_assert(offsetof(struct vif_sta_info, rssi) == 24, "");
static_assert(offsetof(struct vif_sta_info, mm_retry) == 30, "");

static_assert(sizeof(struct vif_ap_info) == 124, "the AP union arm is 124 bytes in the archive");
static_assert(offsetof(struct vif_ap_info, bcn_len) == 100, "");
static_assert(offsetof(struct vif_ap_info, bcn_tbtt_ratio) == 108, "");
static_assert(offsetof(struct vif_ap_info, csa_oft) == 117, "");
static_assert(offsetof(struct vif_ap_info, ctrl_port_ethertype) == 120, "");

static_assert(sizeof(struct vif_info_tag) == 816, "vif_info_tag is 816 bytes in the archive");
static_assert(offsetof(struct vif_info_tag, bssid) == 64, "");
static_assert(offsetof(struct vif_info_tag, mac_addr) == 88, "");
static_assert(offsetof(struct vif_info_tag, type) == 94, "");
static_assert(offsetof(struct vif_info_tag, active) == 96, "");
static_assert(offsetof(struct vif_info_tag, u) == 100, "");
static_assert(offsetof(struct vif_info_tag, sta_list) == 224, "");
static_assert(offsetof(struct vif_info_tag, priv) == 812, "");

// A common symbol in the archive, so it links in whether or not anything
// references it. The archive owns every byte.
extern struct vif_info_tag vif_info_tab[NX_VIRT_DEV_MAX];

// The manager's own state. An interface reaches used_list when the LMAC accepts
// an MM_ADD_IF_REQ for it and leaves on MM_REMOVE_IF_REQ, so an empty used_list
// is the archive's own answer to whether any interface exists yet.
struct vif_mgmt_env_tag {
    struct co_list free_list;       // 0
    struct co_list used_list;       // 8
    uint8_t        vif_sta_cnt;     // 16
    uint8_t        vif_ap_cnt;      // 17
    uint8_t        low_bcn_int_idx; // 18, the VIF with the shortest beacon interval
    uint8_t        reserved_19;
};

static_assert(sizeof(struct vif_mgmt_env_tag) == 20, "vif_mgmt_env_tag is 20 bytes in the archive");
static_assert(offsetof(struct vif_mgmt_env_tag, used_list) == 8, "");
static_assert(offsetof(struct vif_mgmt_env_tag, vif_sta_cnt) == 16, "");
static_assert(offsetof(struct vif_mgmt_env_tag, vif_ap_cnt) == 17, "");
static_assert(offsetof(struct vif_mgmt_env_tag, low_bcn_int_idx) == 18, "");

extern struct vif_mgmt_env_tag vif_mgmt_env;

#ifdef __cplusplus
}
#endif
