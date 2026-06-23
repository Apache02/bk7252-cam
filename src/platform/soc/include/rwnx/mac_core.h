#pragma once

#include "platform/soc.h"


#define MAC_CORE_BASE_ADDR (0xC0000000)


// RWNX NXMAC 802.11 MAC core register block.
// Total size: 1376 words (0x0000–0x157C), REG_MAC_CORE_SIZE = 1376.
// Source: beken378/driver/common/reg/reg_mac_core.h

typedef volatile struct {

    // 0x000 — SIGNATURE: hardware MAC core signature (read-only)
    uint32_t signature;                    // 0x000

    // 0x004 — VERSION_1: MAC core version information
    union {
        uint32_t v;
        struct {
            uint32_t design_kit_version : 5;  // [4:0]   design-kit version number
            uint32_t reserved_5_6       : 2;  // [6:5]
            uint32_t phy_vif_type       : 8;  // [14:7]  PHY/VIF type
            uint32_t reserved_15        : 1;  // [15]
            uint32_t core_type          : 4;  // [19:16] MAC core type
            uint32_t reserved_20_31     : 12; // [31:20]
        };
    } version_1;                          // 0x004

    // 0x008 — VERSION_2: MAC core release information
    union {
        uint32_t v;
        struct {
            uint32_t um_version      : 7;  // [6:0]   micro-management version
            uint32_t ie_release      : 1;  // [7]     IE release flag
            uint32_t release_number  : 6;  // [13:8]  release number
            uint32_t phase_number    : 3;  // [16:14] phase number
            uint32_t reserved_17_31  : 15; // [31:17]
        };
    } version_2;                          // 0x008

    // 0x00C — BITMAP_CNT: TX buffer bitmap count
    union {
        uint32_t v;
        struct {
            uint32_t bitmap_cnt     : 16; // [15:0]  number of bitmap entries
            uint32_t reserved_16_31 : 16; // [31:16]
        };
    } bitmap_cnt;                         // 0x00C

    // 0x010 — MAC_ADDR_LOW: local STA MAC address bits [31:0]
    uint32_t mac_addr_low;                // 0x010

    // 0x014 — MAC_ADDR_HI: local STA MAC address bits [47:32]
    union {
        uint32_t v;
        struct {
            uint32_t mac_addr_high  : 16; // [15:0]  upper 16 bits of MAC address
            uint32_t reserved_16_31 : 16; // [31:16]
        };
    } mac_addr_hi;                        // 0x014

    // 0x018 — MAC_ADDR_LOW_MASK: MAC address mask bits [31:0]
    uint32_t mac_addr_low_mask;           // 0x018

    // 0x01C — MAC_ADDR_HI_MASK: MAC address mask bits [47:32]
    union {
        uint32_t v;
        struct {
            uint32_t mac_addr_high_mask : 16; // [15:0]  upper 16 bits of MAC address mask
            uint32_t reserved_16_31     : 16; // [31:16]
        };
    } mac_addr_hi_mask;                   // 0x01C

    // 0x020 — BSS_ID_LOW: BSS identifier bits [31:0]
    uint32_t bss_id_low;                  // 0x020

    // 0x024 — BSS_ID_HI: BSS identifier bits [47:32]
    union {
        uint32_t v;
        struct {
            uint32_t bss_id_high    : 16; // [15:0]  upper 16 bits of BSS ID
            uint32_t reserved_16_31 : 16; // [31:16]
        };
    } bss_id_hi;                          // 0x024

    // 0x028 — BSS_ID_LOW_MASK: BSS ID mask bits [31:0]
    uint32_t bss_id_low_mask;             // 0x028

    // 0x02C — BSS_ID_HI_MASK: BSS ID mask bits [47:32]
    union {
        uint32_t v;
        struct {
            uint32_t bss_id_high_mask : 16; // [15:0]  upper 16 bits of BSS ID mask
            uint32_t reserved_16_31   : 16; // [31:16]
        };
    } bss_id_hi_mask;                     // 0x02C

    uint32_t reserved_0x30_0x34[2];      // 0x030–0x034

    // 0x038 — STATE_CNTRL: MAC state-machine control
    union {
        uint32_t v;
        struct {
            uint32_t current_state  : 4;  // [3:0]   current MAC hardware state (read-only)
            uint32_t next_state     : 4;  // [7:4]   next MAC hardware state to transition to
            uint32_t reserved_8_31  : 24; // [31:8]
        };
    } state_cntrl;                        // 0x038

    // 0x03C — SCAN_CNTRL: scan control
    union {
        uint32_t v;
        struct {
            uint32_t probe_delay    : 16; // [15:0]  probe delay in microseconds
            uint32_t reserved_16_31 : 16; // [31:16]
        };
    } scan_cntrl;                         // 0x03C

    uint32_t reserved_0x40;              // 0x040

    // 0x044 — DOZE_CNTRL_1: power-save doze control
    union {
        uint32_t v;
        struct {
            uint32_t listen_interval  : 16; // [15:0]  listen interval (beacon periods)
            uint32_t wakeup_dtim      : 1;  // [16]    wake up on DTIM beacon
            uint32_t atim_w           : 15; // [31:17] ATIM window duration (TU)
        };
    } doze_cntrl_1;                       // 0x044

    uint32_t reserved_0x48;              // 0x048

    // 0x04C — MAC_CNTRL_1: main MAC control register
    union {
        uint32_t v;
        struct {
            uint32_t bss_type              : 1;  // [0]    0 = infrastructure, 1 = IBSS
            uint32_t ap                    : 1;  // [1]    1 = operating as AP
            uint32_t pwr_mgt               : 1;  // [2]    power management enable
            uint32_t cfp_aware             : 1;  // [3]    contention-free period awareness
            uint32_t reserved_4            : 1;  // [4]
            uint32_t lp_clk_32786hz        : 1;  // [5]    low-power 32.786 kHz clock select
            uint32_t enable_lp_clk_switch  : 1;  // [6]    enable LP clock switch
            uint32_t active_clk_gating     : 1;  // [7]    active clock gating enable
            uint32_t disable_ack_resp      : 1;  // [8]    disable ACK response
            uint32_t disable_cts_resp      : 1;  // [9]    disable CTS response
            uint32_t disable_ba_resp       : 1;  // [10]   disable block-ACK response
            uint32_t rate_controller_mpif  : 1;  // [11]   rate controller via MPIF
            uint32_t mib_table_reset       : 1;  // [12]   reset MIB table; self-clearing
            uint32_t key_sto_ram_reset     : 1;  // [13]   reset key storage RAM; self-clearing
            uint32_t abgn_mode             : 3;  // [16:14] 802.11 mode: 0=b, 1=a/g, 2=n, ...
            uint32_t reserved_17_23        : 7;  // [23:17]
            uint32_t tsf_updated_by_sw     : 1;  // [24]   TSF timer updated by software
            uint32_t tsf_mgt_disable       : 1;  // [25]   disable TSF management
            uint32_t rx_rifs_en            : 1;  // [26]   enable RX RIFS detection
            uint32_t reserved_27_31        : 5;  // [31:27]
        };
    } mac_cntrl_1;                        // 0x04C

    uint32_t reserved_0x50;              // 0x050

    // 0x054 — MAC_ERR_REC_CNTRL: error recovery control
    union {
        uint32_t v;
        struct {
            uint32_t use_err_rec           : 1;  // [0]    enable error recovery
            uint32_t use_err_det           : 1;  // [1]    enable error detection
            uint32_t hw_fsm_reset          : 1;  // [2]    hardware FSM reset; self-clearing
            uint32_t rx_fifo_reset         : 1;  // [3]    RX FIFO reset; self-clearing
            uint32_t tx_fifo_reset         : 1;  // [4]    TX FIFO reset; self-clearing
            uint32_t mac_phy_if_fifo_reset : 1;  // [5]    MAC-PHY interface FIFO reset; self-clearing
            uint32_t encr_rx_fifo_reset    : 1;  // [6]    encryption RX FIFO reset; self-clearing
            uint32_t ba_ps_bitmap_reset    : 1;  // [7]    BA power-save bitmap reset; self-clearing
            uint32_t reserved_8_15         : 8;  // [15:8]
            uint32_t rx_flow_cntrl_en      : 1;  // [16]   RX flow control enable
            uint32_t reserved_17_31        : 15; // [31:17]
        };
    } mac_err_rec_cntrl;                  // 0x054

    // 0x058 — MAC_ERR_SET_STATUS: error status set (write-1-to-set)
    union {
        uint32_t v;
        struct {
            uint32_t err_in_tx_level1     : 1;  // [0]    error in TX level-1 FSM
            uint32_t err_in_rx_level1     : 1;  // [1]    error in RX level-1 FSM
            uint32_t err_in_tx_rx_level2  : 1;  // [2]    error in TX/RX level-2 FSM
            uint32_t err_in_hw_level3     : 1;  // [3]    error in HW level-3 FSM
            uint32_t reserved_4_31        : 28; // [31:4]
        };
    } mac_err_set_status;                 // 0x058

    // 0x05C — MAC_ERR_CLEAR_STATUS: error status clear (write-1-to-clear)
    union {
        uint32_t v;
        struct {
            uint32_t clear_err_in_tx_level1    : 1;  // [0]    clear TX level-1 error
            uint32_t clear_err_in_rx_level1    : 1;  // [1]    clear RX level-1 error
            uint32_t clear_err_in_tx_rx_level2 : 1;  // [2]    clear TX/RX level-2 error
            uint32_t clear_err_in_hw_level3    : 1;  // [3]    clear HW level-3 error
            uint32_t reserved_4_31             : 28; // [31:4]
        };
    } mac_err_clear_status;               // 0x05C

    // 0x060 — RX_CNTRL: receive control (all bits are independent flags)
    union {
        uint32_t v;
        struct {
            uint32_t exc_unencrypted          : 1; // [0]    exclude unencrypted frames
            uint32_t exc_unencrypted_mpdu     : 1; // [1]    exclude unencrypted MPDUs
            uint32_t dont_dec_ra              : 1; // [2]    do not decrement RA filter
            uint32_t accept_unknown           : 1; // [3]    accept unknown frame types
            uint32_t accept_other_bss         : 1; // [4]    accept frames from other BSSs
            uint32_t accept_error_frames      : 1; // [5]    accept errored frames
            uint32_t accept_undecryptable     : 1; // [6]    accept undecryptable frames
            uint32_t accept_qo_s_null         : 1; // [7]    accept QoS null frames
            uint32_t accept_qc_f_wo_data      : 1; // [8]    accept QCF frames without data
            uint32_t accept_q_data            : 1; // [9]    accept QData frames
            uint32_t accept_cf_wo_data        : 1; // [10]   accept CF frames without data
            uint32_t accept_data              : 1; // [11]   accept data frames
            uint32_t accept_other_mgmt_frames : 1; // [12]   accept other management frames
            uint32_t accept_bfm_frames        : 1; // [13]   accept beamforming frames
            uint32_t accept_all_beacon        : 1; // [14]   accept all beacon frames
            uint32_t accept_not_expected_ba   : 1; // [15]   accept unexpected BA frames
            uint32_t accept_decrypt_error_frames : 1; // [16] accept decryption-error frames
            uint32_t accept_beacon            : 1; // [17]   accept beacon frames
            uint32_t accept_probe_resp        : 1; // [18]   accept probe responses
            uint32_t accept_probe_req         : 1; // [19]   accept probe requests
            uint32_t accept_my_unicast        : 1; // [20]   accept unicast to my address
            uint32_t accept_multicast         : 1; // [21]   accept multicast frames
            uint32_t accept_broadcast         : 1; // [22]   accept broadcast frames
            uint32_t accept_other_data_frames : 1; // [23]   accept other data frames
            uint32_t accept_ps_poll           : 1; // [24]   accept PS-Poll frames
            uint32_t accept_rts               : 1; // [25]   accept RTS frames
            uint32_t accept_cts               : 1; // [26]   accept CTS frames
            uint32_t accept_ack               : 1; // [27]   accept ACK frames
            uint32_t accept_cf_end            : 1; // [28]   accept CF-End frames
            uint32_t accept_ba               : 1; // [29]   accept block ACK frames
            uint32_t accept_bar              : 1; // [30]   accept BA request frames
            uint32_t en_duplicate_detection  : 1; // [31]   enable duplicate frame detection
        };
    } rx_cntrl;                           // 0x060

    // 0x064 — BCN_CNTRL_1: beacon control part 1
    union {
        uint32_t v;
        struct {
            uint32_t beacon_int        : 16; // [15:0]  beacon interval (TU)
            uint32_t imp_tbtt_period   : 7;  // [22:16] impending TBTT period (TU)
            uint32_t imp_tbtt_in128us  : 1;  // [23]    impending TBTT period in 128 μs units
            uint32_t no_bcn_tx_time    : 8;  // [31:24] no-beacon TX time (TU)
        };
    } bcn_cntrl_1;                        // 0x064

    // 0x068 — BCN_CNTRL_2: beacon control part 2
    union {
        uint32_t v;
        struct {
            uint32_t bcn_update_offset : 8;  // [7:0]   beacon update offset (words)
            uint32_t tim_offset        : 8;  // [15:8]  TIM element offset in beacon frame
            uint32_t aid               : 16; // [31:16] association identifier
        };
    } bcn_cntrl_2;                        // 0x068

    uint32_t reserved_0x6c_0x8c[9];      // 0x06C–0x08C

    // 0x090 — DTIM_CFP_1: DTIM and CFP period control
    union {
        uint32_t v;
        struct {
            uint32_t dtim_period       : 8;  // [7:0]   DTIM period (beacon intervals)
            uint32_t cfp_period        : 8;  // [15:8]  CFP period (DTIM intervals)
            uint32_t reserved_16_30    : 15; // [30:16]
            uint32_t dtim_updated_by_sw : 1; // [31]    DTIM counter updated by software
        };
    } dtim_cfp_1;                         // 0x090

    // 0x094 — DTIM_CFP_2: CFP max duration
    union {
        uint32_t v;
        struct {
            uint32_t cfp_max_duration : 16; // [15:0]  maximum CFP duration (TU)
            uint32_t reserved_16_31   : 16; // [31:16]
        };
    } dtim_cfp_2;                         // 0x094

    // 0x098 — RETRY_LIMITS: dot11 short and long retry limits
    union {
        uint32_t v;
        struct {
            uint32_t dot11_short_retry_limit : 8;  // [7:0]   dot11ShortRetryLimit
            uint32_t dot11_long_retry_limit  : 8;  // [15:8]  dot11LongRetryLimit
            uint32_t reserved_16_31          : 16; // [31:16]
        };
    } retry_limits;                       // 0x098

    // 0x09C — BB_SERVICE: baseband service field values
    union {
        uint32_t v;
        struct {
            uint32_t bb_service_a    : 16; // [15:0]  802.11a/g service field
            uint32_t bb_service_b    : 8;  // [23:16] 802.11b service field
            uint32_t reserved_24_25  : 2;  // [25:24]
            uint32_t max_phy_n_tx    : 3;  // [28:26] maximum number of PHY TX chains
            uint32_t reserved_29_31  : 3;  // [31:29]
        };
    } bb_service;                         // 0x09C

    // 0x0A0 — DSSSMAX_POWER_LEVEL: DSSS maximum power level (full 32-bit register).
    // SDK alias: MAX_POWER_LEVEL at same offset provides an alternative packed-byte view
    // with four 8-bit per-rate fields; treat as plain 32-bit here.
    uint32_t dsss_max_pwr_level;          // 0x0A0

    // 0x0A4 — OFDMMAX_POWER_LEVEL: OFDM maximum power level
    uint32_t ofdm_max_pwr_level;          // 0x0A4

    uint32_t reserved_0xa8;              // 0x0A8

    // 0x0AC–0x0B8 — ENCR_KEY_0..3: encryption key words
    uint32_t encr_key[4];                 // 0x0AC–0x0B8

    // 0x0BC — ENCR_MAC_ADDR_LOW: encryption key RAM — MAC address bits [31:0]
    uint32_t encr_mac_addr_low;           // 0x0BC

    // 0x0C0 — ENCR_MAC_ADDR_HIGH: encryption key RAM — MAC address bits [47:32]
    union {
        uint32_t v;
        struct {
            uint32_t mac_addr_ram_high : 16; // [15:0]  upper 16 bits of key-RAM MAC address
            uint32_t reserved_16_31    : 16; // [31:16]
        };
    } encr_mac_addr_high;                 // 0x0C0

    // 0x0C4 — ENCR_CNTRL: encryption key RAM control
    union {
        uint32_t v;
        struct {
            uint32_t c_len_ram        : 1;  // [0]    cipher length (0=128-bit, 1=256-bit)
            uint32_t use_def_key_ram  : 1;  // [1]    use default key from RAM
            uint32_t spp_ram          : 2;  // [3:2]  AMSDU SPP indicator
            uint32_t vlan_id_ram      : 4;  // [7:4]  VLAN ID
            uint32_t c_type_ram       : 3;  // [10:8] cipher type
            uint32_t reserved_11_15   : 5;  // [15:11]
            uint32_t key_index_ram    : 10; // [25:16] key index in key storage RAM
            uint32_t reserved_26_27   : 2;  // [27:26]
            uint32_t search_error     : 1;  // [28]   key search error (read-only)
            uint32_t new_search       : 1;  // [29]   trigger new key search; self-clearing
            uint32_t new_write        : 1;  // [30]   trigger new key write; self-clearing
            uint32_t new_read         : 1;  // [31]   trigger new key read; self-clearing
        };
    } encr_cntrl;                         // 0x0C4

    // 0x0C8–0x0D4 — ENCR_WPI_INT_KEY_0..3: WPI integrity key words
    uint32_t encr_wpi_int_key[4];         // 0x0C8–0x0D4

    // 0x0D8 — ENCR_RAM_CONFIG: key storage RAM configuration
    union {
        uint32_t v;
        struct {
            uint32_t sta_key_start_index : 8;  // [7:0]   first STA key index in RAM
            uint32_t sta_key_end_index   : 8;  // [15:8]  last STA key index in RAM
            uint32_t n_vap               : 4;  // [19:16] number of virtual APs
            uint32_t reserved_20_23      : 4;  // [23:20]
            uint32_t sta_key_max_index   : 8;  // [31:24] maximum STA key index
        };
    } encr_ram_config;                    // 0x0D8

    // 0x0DC — RATES: BSS basic rate set
    union {
        uint32_t v;
        struct {
            uint32_t bss_basic_rate_set : 12; // [11:0]  basic rate set bitmap
            uint32_t reserved_12_31     : 20; // [31:12]
        };
    } rates;                              // 0x0DC

    // 0x0E0 — OLBC: overlapping legacy BSS condition
    union {
        uint32_t v;
        struct {
            uint32_t olbc_timer  : 16; // [15:0]  OLBC timer value (TU)
            uint32_t ofdm_count  : 8;  // [23:16] OFDM OLBC detection count
            uint32_t dsss_count  : 8;  // [31:24] DSSS OLBC detection count
        };
    } olbc;                               // 0x0E0

    // 0x0E4 — TIMINGS_1: RF delay timings
    union {
        uint32_t v;
        struct {
            uint32_t mac_core_clk_freq        : 8;  // [7:0]   MAC core clock frequency (MHz)
            uint32_t tx_rf_delay_in_mac_clk   : 10; // [17:8]  TX RF turn-on delay (MAC clocks)
            uint32_t tx_chain_delay_in_mac_clk: 10; // [27:18] TX chain delay (MAC clocks)
            uint32_t reserved_28_31           : 4;  // [31:28]
        };
    } timings_1;                          // 0x0E4

    // 0x0E8 — TIMINGS_2: slot time
    union {
        uint32_t v;
        struct {
            uint32_t slot_time           : 8;  // [7:0]   slot time (μs)
            uint32_t slot_time_in_mac_clk: 16; // [23:8]  slot time (MAC clocks)
            uint32_t reserved_24_31      : 8;  // [31:24]
        };
    } timings_2;                          // 0x0E8

    // 0x0EC — TIMINGS_3: MAC processing and TX delays
    union {
        uint32_t v;
        struct {
            uint32_t mac_proc_delay_in_mac_clk  : 10; // [9:0]   MAC processing delay (MAC clocks)
            uint32_t tx_delay_rf_on_in_mac_clk  : 10; // [19:10] TX delay with RF on (MAC clocks)
            uint32_t rx_rf_delay_in_mac_clk     : 10; // [29:20] RX RF turn-on delay (MAC clocks)
            uint32_t reserved_30_31             : 2;  // [31:30]
        };
    } timings_3;                          // 0x0EC

    // 0x0F0 — TIMINGS_4: radio wake-up and chirp timings
    union {
        uint32_t v;
        struct {
            uint32_t wt2_crypt_clk_ratio : 2;  // [1:0]   write-to-crypt clock ratio
            uint32_t reserved_2_11       : 10; // [11:2]
            uint32_t radio_chirp_time    : 10; // [21:12] radio chirp time (MAC clocks)
            uint32_t radio_wake_up_time  : 10; // [31:22] radio wake-up time (MAC clocks)
        };
    } timings_4;                          // 0x0F0

    // 0x0F4 — TIMINGS_5: SIFS B
    union {
        uint32_t v;
        struct {
            uint32_t sifs_b              : 8;  // [7:0]   SIFS-B duration (μs)
            uint32_t sifs_b_in_mac_clk   : 16; // [23:8]  SIFS-B duration (MAC clocks)
            uint32_t reserved_24_31      : 8;  // [31:24]
        };
    } timings_5;                          // 0x0F4

    // 0x0F8 — TIMINGS_6: SIFS A
    union {
        uint32_t v;
        struct {
            uint32_t sifs_a              : 8;  // [7:0]   SIFS-A duration (μs)
            uint32_t sifs_a_in_mac_clk   : 16; // [23:8]  SIFS-A duration (MAC clocks)
            uint32_t reserved_24_31      : 8;  // [31:24]
        };
    } timings_6;                          // 0x0F8

    // 0x0FC — TIMINGS_7: RIFS and CCA delay
    union {
        uint32_t v;
        struct {
            uint32_t rifs          : 8;  // [7:0]   RIFS duration (μs)
            uint32_t rx_cca_delay  : 4;  // [11:8]  RX CCA delay (MAC clocks)
            uint32_t reserved_12_31: 20; // [31:12]
        };
    } timings_7;                          // 0x0FC

    // 0x100 — TIMINGS_8: RX start delays per PHY type
    union {
        uint32_t v;
        struct {
            uint32_t rx_start_delay_ofdm  : 8; // [7:0]   RX start delay OFDM (MAC clocks)
            uint32_t rx_start_delay_long  : 8; // [15:8]  RX start delay long preamble (MAC clocks)
            uint32_t rx_start_delay_short : 8; // [23:16] RX start delay short preamble (MAC clocks)
            uint32_t rx_start_delay_mimo  : 8; // [31:24] RX start delay MIMO (MAC clocks)
        };
    } timings_8;                          // 0x100

    // 0x104 — TIMINGS_9: DMA processing delay and RIFS in MAC clocks
    union {
        uint32_t v;
        struct {
            uint32_t tx_dma_proc_dly_in_mac_clk : 10; // [9:0]   TX DMA processing delay (MAC clocks)
            uint32_t rifs_in_mac_clk            : 10; // [19:10] RIFS duration (MAC clocks)
            uint32_t rifs_to_in_mac_clk         : 10; // [29:20] RIFS timeout (MAC clocks)
            uint32_t reserved_30_31             : 2;  // [31:30]
        };
    } timings_9;                          // 0x104

    uint32_t reserved_0x108;             // 0x108

    // 0x10C — PROT_TRIG_TIMER: protection trigger timers
    union {
        uint32_t v;
        struct {
            uint32_t edca_trigger_timer : 8;  // [7:0]   EDCA trigger timer (μs)
            uint32_t hcca_trigger_timer : 8;  // [15:8]  HCCA trigger timer (μs)
            uint32_t reserved_16_31     : 16; // [31:16]
        };
    } prot_trig_timer;                    // 0x10C

    // 0x110 — TX_TRIGGER_TIMER: TX timeout timers
    union {
        uint32_t v;
        struct {
            uint32_t tx_absolute_timeout : 8;  // [7:0]   TX absolute timeout (μs)
            uint32_t tx_packet_timeout   : 8;  // [15:8]  TX packet timeout (μs)
            uint32_t reserved_16_31      : 16; // [31:16]
        };
    } tx_trigger_timer;                   // 0x110

    // 0x114 — RX_TRIGGER_TIMER: RX timeout timers
    union {
        uint32_t v;
        struct {
            uint32_t rx_absolute_timeout    : 8;  // [7:0]   RX absolute timeout (μs)
            uint32_t rx_packet_timeout      : 8;  // [15:8]  RX packet timeout (μs)
            uint32_t rx_payload_used_count  : 8;  // [23:16] RX payload buffer used count
            uint32_t reserved_24_31         : 8;  // [31:24]
        };
    } rx_trigger_timer;                   // 0x114

    // 0x118 — MIB_TABLE_WRITE: MIB table write access
    union {
        uint32_t v;
        struct {
            uint32_t mib_table_index      : 10; // [9:0]   MIB counter index
            uint32_t reserved_10_13       : 4;  // [13:10]
            uint32_t mib_increment_mode   : 1;  // [14]    1 = increment; 0 = absolute write
            uint32_t mib_write            : 1;  // [15]    trigger MIB write; self-clearing
            uint32_t mib_value            : 16; // [31:16] value to write to MIB counter
        };
    } mib_table_write;                    // 0x118

    // 0x11C — MONOTONIC_COUNTER_1: monotonic counter 1 (32-bit)
    uint32_t monotonic_counter_1;         // 0x11C

    // 0x120 — MONOTONIC_COUNTER_2_LO: monotonic counter 2 low word
    uint32_t monotonic_counter_2_lo;      // 0x120

    // 0x124 — MONOTONIC_COUNTER_2_HI: monotonic counter 2 high word
    union {
        uint32_t v;
        struct {
            uint32_t monotonic_counter_high2      : 16; // [15:0]  upper 16 bits of counter 2
            uint32_t reserved_16_30               : 15; // [30:16]
            uint32_t monotonic_counter2_sw_update : 1;  // [31]    software update trigger; self-clearing
        };
    } monotonic_counter_2_hi;             // 0x124

    // 0x128–0x14C — ABS_TIMER[10]: absolute general-purpose timers (10 words)
    uint32_t abs_timer[10];               // 0x128–0x14C

    // 0x150 — MAX_RX_LENGTH: maximum allowed receive MPDU length
    union {
        uint32_t v;
        struct {
            uint32_t max_allowed_length : 20; // [19:0]  maximum receive MPDU length (bytes)
            uint32_t reserved_20_31     : 12; // [31:20]
        };
    } max_rx_length;                      // 0x150

    uint32_t reserved_0x154_0x1fc[43];   // 0x154–0x1FC

    // 0x200–0x20C — EDCA_AC_0..3: EDCA access category parameters
    union {
        uint32_t v;
        struct {
            uint32_t aifsn       : 4;  // [3:0]   AIFS number
            uint32_t cw_min      : 4;  // [7:4]   CWmin (encoded as log2(CWmin+1))
            uint32_t cw_max      : 4;  // [11:8]  CWmax (encoded as log2(CWmax+1))
            uint32_t tx_op_limit : 16; // [27:12] TXOP limit (32 μs units)
            uint32_t reserved_28_31 : 4; // [31:28]
        };
    } edca_ac[4];                         // 0x200–0x20C

    uint32_t reserved_0x210_0x21c[4];    // 0x210–0x21C

    // 0x220 — EDCA_CCA_BUSY: EDCA CCA busy time accumulator (read-only)
    uint32_t edca_cca_busy;               // 0x220

    // 0x224 — EDCA_CNTRL: EDCA control
    union {
        uint32_t v;
        struct {
            uint32_t send_cf_end_now     : 1;  // [0]    immediately send CF-End
            uint32_t send_cf_end         : 1;  // [1]    send CF-End at TXOP boundary
            uint32_t reserved_2_3        : 2;  // [3:2]
            uint32_t rem_txop_in_dur_field: 1; // [4]    remaining TXOP in duration field
            uint32_t keep_txop_open      : 1;  // [5]    keep TXOP open
            uint32_t reserved_6_31       : 26; // [31:6]
        };
    } edca_cntrl;                         // 0x224

    uint32_t reserved_0x228_0x27c[22];   // 0x228–0x27C

    // 0x280 — QUIET_ELEMENT_1A: first quiet element period/duration
    union {
        uint32_t v;
        struct {
            uint32_t quiet_count_1    : 8;  // [7:0]   quiet count 1 (beacon intervals until quiet)
            uint32_t quiet_period_1   : 8;  // [15:8]  quiet period 1 (beacon intervals)
            uint32_t quiet_duration_1 : 16; // [31:16] quiet duration 1 (TU)
        };
    } quiet_element_1a;                   // 0x280

    // 0x284 — QUIET_ELEMENT_1B: first quiet element offset
    union {
        uint32_t v;
        struct {
            uint32_t quiet_offset_1 : 16; // [15:0]  quiet offset 1 (TU from TBTT)
            uint32_t reserved_16_31 : 16; // [31:16]
        };
    } quiet_element_1b;                   // 0x284

    // 0x288 — QUIET_ELEMENT_2A: second quiet element period/duration
    union {
        uint32_t v;
        struct {
            uint32_t quiet_count_2    : 8;  // [7:0]   quiet count 2
            uint32_t quiet_period_2   : 8;  // [15:8]  quiet period 2
            uint32_t quiet_duration_2 : 16; // [31:16] quiet duration 2 (TU)
        };
    } quiet_element_2a;                   // 0x288

    // 0x28C — QUIET_ELEMENT_2B: second quiet element offset
    union {
        uint32_t v;
        struct {
            uint32_t quiet_offset_2 : 16; // [15:0]  quiet offset 2 (TU from TBTT)
            uint32_t reserved_16_31 : 16; // [31:16]
        };
    } quiet_element_2b;                   // 0x28C

    // 0x290 — ADD_CCA_BUSY_SEC_20: additional CCA busy time — secondary 20 MHz channel
    uint32_t add_cca_busy_sec20;          // 0x290

    // 0x294 — ADD_CCA_BUSY_SEC_40: additional CCA busy time — secondary 40 MHz channel
    uint32_t add_cca_busy_sec40;          // 0x294

    // 0x298 — ADD_CCA_BUSY_SEC_80: additional CCA busy time — secondary 80 MHz channel
    uint32_t add_cca_busy_sec80;          // 0x298

    uint32_t reserved_0x29c_0x2fc[25];   // 0x29C–0x2FC

    // 0x300 — STBC_CNTRL: STBC and dual-CTS protection control
    union {
        uint32_t v;
        struct {
            uint32_t cf_end_stbc_dur : 16; // [15:0]  CF-End STBC duration (μs)
            uint32_t cts_stbc_dur    : 8;  // [23:16] CTS STBC duration (μs)
            uint32_t dual_cts_prot   : 1;  // [24]    dual CTS protection enable
            uint32_t basic_stbc_mcs  : 7;  // [31:25] basic STBC MCS index
        };
    } stbc_cntrl;                         // 0x300

    // 0x304 — START_TX_1: start-transmit control part 1
    union {
        uint32_t v;
        struct {
            uint32_t start_tx_frame_ex    : 1;  // [0]    start TX exchange
            uint32_t start_tx_frm_ex_type : 2;  // [2:1]  TX exchange type
            uint32_t start_tx_ac          : 2;  // [4:3]  access category for TX
            uint32_t reserved_5           : 1;  // [5]
            uint32_t start_tx_ksr_index   : 10; // [15:6]  key storage RAM index
            uint32_t start_tx_mcs_index0  : 8;  // [23:16] MCS index for first TX
            uint32_t start_tx_format_mod  : 3;  // [26:24] TX format/modulation
            uint32_t start_tx_pre_type    : 1;  // [27]    preamble type
            uint32_t start_tx_bw          : 2;  // [29:28] TX bandwidth
            uint32_t reserved_30_31       : 2;  // [31:30]
        };
    } start_tx_1;                         // 0x304

    // 0x308 — START_TX_2: start-transmit duration control
    union {
        uint32_t v;
        struct {
            uint32_t dur_control_frm : 16; // [15:0]  duration for control frame (μs)
            uint32_t reserved_16_31  : 16; // [31:16]
        };
    } start_tx_2;                         // 0x308

    // 0x30C — START_TX_3: start-transmit MIMO/STBC parameters
    union {
        uint32_t v;
        struct {
            uint32_t start_tx_num_extn_ss : 2;  // [1:0]   number of extension spatial streams
            uint32_t start_tx_stbc        : 2;  // [3:2]   STBC coding
            uint32_t start_tx_fec_coding  : 1;  // [4]     FEC coding (0=BCC, 1=LDPC)
            uint32_t start_tx_n_tx        : 3;  // [7:5]   number of TX chains
            uint32_t start_tx_short_gi    : 1;  // [8]     short guard interval
            uint32_t reserved_9_11        : 3;  // [11:9]
            uint32_t start_tx_smoothing   : 1;  // [12]    smoothing enable
            uint32_t start_tx_lstp        : 1;  // [13]    L-SIG TXOP protection
            uint32_t reserved_14_15       : 2;  // [15:14]
            uint32_t start_tx_antenna_set : 8;  // [23:16] antenna set bitmap
            uint32_t start_tx_smm_index   : 8;  // [31:24] spatial mapping matrix index
        };
    } start_tx_3;                         // 0x30C

    // 0x310 — TX_BW_CNTRL: TX bandwidth control
    union {
        uint32_t v;
        struct {
            uint32_t default_bw_txop_v        : 1;  // [0]    default bandwidth TXOP valid
            uint32_t default_bw_txop          : 2;  // [2:1]  default bandwidth for TXOP
            uint32_t drop_to_lower_bw         : 1;  // [3]    drop to lower bandwidth
            uint32_t num_try_bw_acquisition   : 3;  // [6:4]  number of BW acquisition attempts
            uint32_t dyn_bw_en                : 1;  // [7]    dynamic bandwidth enable
            uint32_t appdu_max_time           : 8;  // [15:8] A-PPDU maximum time (μs × 4)
            uint32_t max_supported_bw         : 2;  // [17:16] maximum supported bandwidth
            uint32_t reserved_18_31           : 14; // [31:18]
        };
    } tx_bw_cntrl;                        // 0x310

    // 0x314 — HTMCS: HT MCS set
    union {
        uint32_t v;
        struct {
            uint32_t bss_basic_ht_mcs_set_em : 16; // [15:0]  BSS basic HT MCS set (equal modulation)
            uint32_t bss_basic_ht_mcs_set_um : 6;  // [21:16] BSS basic HT MCS set (unequal modulation)
            uint32_t reserved_22_31          : 10; // [31:22]
        };
    } htmcs;                              // 0x314

    uint32_t reserved_0x318;             // 0x318

    // 0x31C — VHTMCS: VHT MCS set
    union {
        uint32_t v;
        struct {
            uint32_t bss_basic_vht_mcs_set : 16; // [15:0]  BSS basic VHT MCS set
            uint32_t reserved_16_31        : 16; // [31:16]
        };
    } vhtmcs;                             // 0x31C

    // 0x320 — LSTP: L-SIG TXOP protection support
    union {
        uint32_t v;
        struct {
            uint32_t support_lstp   : 1;  // [0]    1 = L-SIG TXOP protection supported
            uint32_t reserved_1_31  : 31; // [31:1]
        };
    } lstp;                               // 0x320

    uint32_t reserved_0x324_0x34c[11];   // 0x324–0x34C

    // 0x350 — BFMEE_CONTROL: beamformee control
    // Note: gated by RW_BFMEE_EN in the SDK; included unconditionally here
    // since this is a hardware layout header.
    union {
        uint32_t v;
        struct {
            uint32_t bfmee_enable      : 1;  // [0]    beamformee enable
            uint32_t bfmee_mu_support  : 1;  // [1]    MU beamformee support
            uint32_t bfmee_codebook    : 1;  // [2]    codebook information
            uint32_t bfmee_grouping    : 2;  // [4:3]  grouping capability
            uint32_t bfmee_nr          : 3;  // [7:5]  number of rows (Nrow)
            uint32_t bfmee_nc          : 3;  // [10:8] number of columns (Ncol)
            uint32_t reserved_11_15    : 5;  // [15:11]
            uint32_t bfr_mcs           : 8;  // [23:16] beamformer MCS index
            uint32_t bfr_short_gi      : 1;  // [24]    beamformer short-GI
            uint32_t bfr_format_mod    : 3;  // [27:25] beamformer format/modulation
            uint32_t reserved_28_31    : 4;  // [31:28]
        };
    } bfmee_control;                      // 0x350

    uint32_t reserved_0x354_0x3fc[43];   // 0x354–0x3FC

    // 0x400 — COEX_CONTROL: coexistence control
    // Note: gated by RW_WLAN_COEX_EN in the SDK; included unconditionally here
    // since this is a hardware layout header.
    union {
        uint32_t v;
        struct {
            uint32_t coex_enable                  : 1;  // [0]    coexistence enable
            uint32_t coex_phy_tx_abort_enable     : 1;  // [1]    PHY TX abort enable
            uint32_t coex_phy_rx_abort_enable     : 1;  // [2]    PHY RX abort enable
            uint32_t coex_postpone_tx_enable      : 1;  // [3]    postpone TX enable
            uint32_t coex_force_enable            : 1;  // [4]    force coexistence
            uint32_t coex_auto_pti_adj_enable     : 1;  // [5]    auto PTI adjustment enable
            uint32_t coex_auto_pti_adj_incr       : 2;  // [7:6]  auto PTI adjustment increment
            uint32_t reserved_8_11                : 4;  // [11:8]
            uint32_t coex_wlan_chan_offset         : 1;  // [12]   WLAN channel offset
            uint32_t reserved_13_15               : 3;  // [15:13]
            uint32_t coex_wlan_chan_freq           : 7;  // [22:16] WLAN channel frequency index
            uint32_t reserved_23                  : 1;  // [23]
            uint32_t coex_force_wlan_tx           : 1;  // [24]   force WLAN TX assertion
            uint32_t coex_force_wlan_rx           : 1;  // [25]   force WLAN RX assertion
            uint32_t coex_force_wlan_chan_bw       : 1;  // [26]   force WLAN channel BW
            uint32_t coex_force_wlan_pti_toggle   : 1;  // [27]   force WLAN PTI toggle
            uint32_t coex_force_wlan_pti          : 4;  // [31:28] forced WLAN PTI value
        };
    } coex_control;                       // 0x400

    // 0x404 — COEX_PTI: coexistence priority type indicator
    union {
        uint32_t v;
        struct {
            uint32_t coex_pti_ack      : 4; // [3:0]   PTI for ACK frames
            uint32_t coex_pti_cntl     : 4; // [7:4]   PTI for control frames
            uint32_t coex_pti_mgt      : 4; // [11:8]  PTI for management frames
            uint32_t coex_pti_vo_data  : 4; // [15:12] PTI for VO data
            uint32_t coex_pti_vi_data  : 4; // [19:16] PTI for VI data
            uint32_t coex_pti_be_data  : 4; // [23:20] PTI for BE data
            uint32_t coex_pti_bk_data  : 4; // [27:24] PTI for BK data
            uint32_t coex_pti_bcn_data : 4; // [31:28] PTI for beacon data
        };
    } coex_pti;                           // 0x404

    // 0x408 — COEX_STAT: coexistence status (read-only)
    union {
        uint32_t v;
        struct {
            uint32_t coex_wlan_tx_abort_state   : 1;  // [0]    WLAN TX abort state
            uint32_t coex_wlan_rx_abort_state   : 1;  // [1]    WLAN RX abort state
            uint32_t coex_wlan_tx_state         : 1;  // [2]    WLAN TX state
            uint32_t coex_wlan_rx_state         : 1;  // [3]    WLAN RX state
            uint32_t coex_wlan_chan_bw_state     : 1;  // [4]    WLAN channel BW state
            uint32_t coex_wlan_pti_toggle_state : 1;  // [5]    WLAN PTI toggle state
            uint32_t reserved_6_7               : 2;  // [7:6]
            uint32_t coex_wlan_pti_state        : 4;  // [11:8]  WLAN PTI state
            uint32_t reserved_12_31             : 20; // [31:12]
        };
    } coex_stat;                          // 0x408

    // 0x40C — COEX_INT: coexistence interrupt enable
    union {
        uint32_t v;
        struct {
            uint32_t coex_wlan_tx_abort_rise_en : 1;  // [0]    TX abort rising-edge interrupt enable
            uint32_t coex_wlan_tx_abort_fall_en : 1;  // [1]    TX abort falling-edge interrupt enable
            uint32_t coex_wlan_rx_abort_rise_en : 1;  // [2]    RX abort rising-edge interrupt enable
            uint32_t coex_wlan_rx_abort_fall_en : 1;  // [3]    RX abort falling-edge interrupt enable
            uint32_t reserved_4_31              : 28; // [31:4]
        };
    } coex_int;                           // 0x40C

    uint32_t reserved_0x410_0x4fc[60];   // 0x410–0x4FC

    // 0x500 — DEBUG_HWSM_1: hardware state machine debug (last state)
    union {
        uint32_t v;
        struct {
            uint32_t rx_control_ls  : 6;  // [5:0]   RX control last state
            uint32_t reserved_6_7   : 2;  // [7:6]
            uint32_t tx_control_ls  : 9;  // [16:8]  TX control last state
            uint32_t reserved_17_23 : 7;  // [23:17]
            uint32_t mac_control_ls : 8;  // [31:24] MAC control last state
        };
    } debug_hwsm_1;                       // 0x500

    // 0x504 — DEBUG_HWSM_2: hardware state machine debug (current state)
    union {
        uint32_t v;
        struct {
            uint32_t rx_control_cs  : 6;  // [5:0]   RX control current state
            uint32_t reserved_6_7   : 2;  // [7:6]
            uint32_t tx_control_cs  : 9;  // [16:8]  TX control current state
            uint32_t reserved_17_23 : 7;  // [23:17]
            uint32_t mac_control_cs : 8;  // [31:24] MAC control current state
        };
    } debug_hwsm_2;                       // 0x504

    uint32_t reserved_0x508;             // 0x508

    // 0x50C — DEBUG_PORT_VALUE: debug port value output (read-only)
    uint32_t debug_port_value;            // 0x50C

    // 0x510 — DEBUG_PORT_SEL: debug port mux select
    union {
        uint32_t v;
        struct {
            uint32_t debug_port_sel1 : 8;  // [7:0]   debug port 1 select
            uint32_t debug_port_sel2 : 8;  // [15:8]  debug port 2 select
            uint32_t reserved_16_31  : 16; // [31:16]
        };
    } debug_port_sel;                     // 0x510

    // 0x514 — DEBUG_NAV: NAV counter debug
    union {
        uint32_t v;
        struct {
            uint32_t nav_counter    : 26; // [25:0]  current NAV counter value (μs, read-only)
            uint32_t reserved_26_31 : 6;  // [31:26]
        };
    } debug_nav;                          // 0x514

    // 0x518 — DEBUG_CW: contention window and backoff debug
    union {
        uint32_t v;
        struct {
            uint32_t current_cw0    : 4;  // [3:0]   current CW for AC0
            uint32_t current_cw1    : 4;  // [7:4]   current CW for AC1
            uint32_t current_cw2    : 4;  // [11:8]  current CW for AC2
            uint32_t current_cw3    : 4;  // [15:12] current CW for AC3
            uint32_t active_ac      : 2;  // [17:16] currently active access category
            uint32_t reserved_18_23 : 6;  // [23:18]
            uint32_t backoff_offset : 2;  // [25:24] backoff slot offset
            uint32_t reserved_26_31 : 6;  // [31:26]
        };
    } debug_cw;                           // 0x518

    // 0x51C — DEBUG_QSRC: per-AC short retry counter debug
    union {
        uint32_t v;
        struct {
            uint32_t ac0_qsrc : 8; // [7:0]   AC0 QSRC
            uint32_t ac1_qsrc : 8; // [15:8]  AC1 QSRC
            uint32_t ac2_qsrc : 8; // [23:16] AC2 QSRC
            uint32_t ac3_qsrc : 8; // [31:24] AC3 QSRC
        };
    } debug_qsrc;                         // 0x51C

    // 0x520 — DEBUG_QLRC: per-AC long retry counter debug
    union {
        uint32_t v;
        struct {
            uint32_t ac0_qlrc : 8; // [7:0]   AC0 QLRC
            uint32_t ac1_qlrc : 8; // [15:8]  AC1 QLRC
            uint32_t ac2_qlrc : 8; // [23:16] AC2 QLRC
            uint32_t ac3_qlrc : 8; // [31:24] AC3 QLRC
        };
    } debug_qlrc;                         // 0x520

    uint32_t reserved_0x524_0x558[14];   // 0x524–0x558

    // 0x55C — DEBUG_PHY: PHY debug control
    union {
        uint32_t v;
        struct {
            uint32_t rx_req_force_deassertion  : 1;  // [0]    force RX request de-assertion
            uint32_t rx_end_for_timing_err_rec : 1;  // [1]    RX end for timing error recovery
            uint32_t reserved_2_31             : 30; // [31:2]
        };
    } debug_phy;                          // 0x55C

    uint32_t reserved_0x560_0x564[2];    // 0x560–0x564

    // 0x568–0x574 — Local and peer TSF timer registers.
    // These four registers appear only as bare ADDR macros (no OFFSET macros)
    // in the SDK; their presence here is inferred from address arithmetic.
    uint32_t local_tsf_low;              // 0x568  local TSF timer bits [31:0]
    uint32_t local_tsf_high;             // 0x56C  local TSF timer bits [63:32]
    uint32_t peer_tsf_low;               // 0x570  peer TSF timer bits [31:0]
    uint32_t peer_tsf_high;              // 0x574  peer TSF timer bits [63:32]

    uint32_t reserved_0x578_0x157c[1026]; // 0x578–0x157C (fills remainder to 1376 words)

} hw_mac_core_t;

#define hw_mac_core ((volatile hw_mac_core_t *)MAC_CORE_BASE_ADDR)
