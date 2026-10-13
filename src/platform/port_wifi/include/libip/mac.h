#pragma once

// ip/mac/mac.h — the 802.11 types the tasks pass around in their messages.
// Layouts are dictated by libip_7221u.a and were read out of its DWARF debug
// info.

#include <assert.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mac_addr {
    uint16_t array[3];
};

struct mac_ssid {
    uint8_t length;
    uint8_t array[32];
};

struct mac_rateset {
    uint8_t length;
    uint8_t array[12];
};

static_assert(sizeof(struct mac_rateset) == 13, "me_task.o DWARF: mac_rateset is 13 bytes");

// MAC_SEC_KEY_LEN = 32. The words are little-endian, so the key bytes can be copied in as they are.
struct mac_sec_key {
    uint8_t  length;
    uint32_t array[8];
};
static_assert(sizeof(struct mac_sec_key) == 36, "mm_task.o DWARF: mac_sec_key is 36 bytes");

// MAX_MCS_LEN = 16.
struct mac_htcapability {
    uint16_t ht_capa_info;
    uint8_t  a_mpdu_param;
    uint8_t  mcs_rate[16];
    uint16_t ht_extended_capa;
    uint32_t tx_beamforming_capa;
    uint8_t  asel_capa;
};

struct mac_vhtcapability {
    uint32_t vht_capa_info;
    uint16_t rx_mcs_map;
    uint16_t rx_highest;
    uint16_t tx_mcs_map;
    uint16_t tx_highest;
};

#ifdef __cplusplus
}
#endif
