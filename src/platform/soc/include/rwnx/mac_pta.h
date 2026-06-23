#pragma once

#include "platform/soc.h"


#define MAC_PTA_BASE_ADDR (0xC0010000)


typedef volatile struct {
    uint32_t reserved_0x00; // no register at base + 0x00

    union {
        uint32_t v;
        struct {
            uint32_t pta_enable : 1;        // [0]
            uint32_t basic_priority : 1;    // [1]
            uint32_t no_sim_tx : 1;         // [2]
            uint32_t no_sim_rx : 1;         // [3]
            uint32_t pti_enable : 1;        // [4]
            uint32_t chan_enable : 1;        // [5]
            uint32_t bt_event_mask : 1;     // [6]
            uint32_t wlan_pti_mode : 1;     // [7]
            uint32_t chan_margin : 6;        // [13:8]
            uint32_t reserved_14_15 : 2;   // [15:14]
            uint32_t sw_bt_tx_abort : 1;    // [16]
            uint32_t sw_bt_rx_abort : 1;    // [17]
            uint32_t sw_wlan_tx_abort : 1;  // [18]
            uint32_t sw_wlan_rx_abort : 1;  // [19]
            uint32_t reserved_20_31 : 12;  // [31:20]
        };
    } config; // 0x04

    uint32_t stat_bt_tx;         // 0x08
    uint32_t stat_bt_abort_tx;   // 0x0C
    uint32_t stat_bt_rx;         // 0x10
    uint32_t stat_bt_rx_abort;   // 0x14
    uint32_t stat_wlan_tx;       // 0x18
    uint32_t stat_wlan_abort_tx; // 0x1C
    uint32_t stat_wlan_rx;       // 0x20
    uint32_t stat_wlan_rx_abort; // 0x24
} hw_mac_pta_t;

#define hw_mac_pta ((volatile hw_mac_pta_t *)MAC_PTA_BASE_ADDR)
