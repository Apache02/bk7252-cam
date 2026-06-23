#pragma once

#include "platform/soc.h"


#define RC_BASE_ADDR (0x01050000)


typedef volatile struct {
    union {
        uint32_t v;
        struct {
            uint32_t ch0_en: 1;          // [0]
            uint32_t reserved_1_2: 2;    // [2:1]
            uint32_t rc_en: 1;           // [3]
            uint32_t reserved_4_7: 4;    // [7:4]
            uint32_t ch0_ld: 1;          // [8]
            uint32_t reserved_9_11: 3;   // [11:9]
            uint32_t ch0_shdn_stat: 1;   // [12]
            uint32_t reserved_13_15: 3;  // [15:13]
            uint32_t rc_state: 3;        // [18:16]
            uint32_t reserved_19_29: 11; // [29:19]
            uint32_t spi_reset: 1;       // [30]
            uint32_t force_enable: 1;    // [31]
        };
    } cntl_stat;          // 0x00

    union {
        uint32_t v;
        struct {
            uint32_t trx_reg_stat: 28; // [27:0]
            uint32_t prescaler: 4;     // [31:28]
        };
    } beken_spi;          // 0x04

    uint32_t reserved_0x08_0x10[3]; // 0x08..0x10

    union {
        uint32_t v;
        struct {
            uint32_t ch0_outpower: 8;   // [7:0]
            uint32_t reserved_8_31: 24; // [31:8]
        };
    } ch0_outpower;       // 0x14

    uint32_t reserved_0x18_0x1c[2]; // 0x18..0x1C

    union {
        uint32_t v;
        struct {
            uint32_t ch0_rx_on_delay: 8;  // [7:0]
            uint32_t reserved_8_15: 8;    // [15:8]
            uint32_t ch0_rx_off_delay: 8; // [23:16]
            uint32_t reserved_24_31: 8;   // [31:24]
        };
    } ch0_rx_onoff_delay; // 0x20

    uint32_t reserved_0x24_0x28[2]; // 0x24..0x28

    union {
        uint32_t v;
        struct {
            uint32_t ch0_tx_on_delay: 8;  // [7:0]
            uint32_t reserved_8_15: 8;    // [15:8]
            uint32_t ch0_tx_off_delay: 8; // [23:16]
            uint32_t reserved_24_31: 8;   // [31:24]
        };
    } ch0_tx_onoff_delay; // 0x2C

    uint32_t reserved_0x30_0x34[2]; // 0x30..0x34

    union {
        uint32_t v;
        struct {
            uint32_t ch0_pa_on_delay: 8;  // [7:0]
            uint32_t reserved_8_15: 8;    // [15:8]
            uint32_t ch0_pa_off_delay: 8; // [23:16]
            uint32_t reserved_24_31: 8;   // [31:24]
        };
    } ch0_pa_onoff_delay; // 0x38

    uint32_t reserved_0x3c_0x40[2]; // 0x3C..0x40

    union {
        uint32_t v;
        struct {
            uint32_t ch0_shdn_on_delay: 8;  // [7:0]
            uint32_t reserved_8_15: 8;      // [15:8]
            uint32_t ch0_shdn_off_delay: 8; // [23:16]
            uint32_t reserved_24_31: 8;     // [31:24]
        };
    } ch0_shdn_onoff_delay; // 0x44

    uint32_t reserved_0x48_0x60[7]; // 0x48..0x60

    union {
        uint32_t v;
        struct {
            uint32_t f_ch0_shdn: 1;    // [0]
            uint32_t f_ch0_rxen: 1;    // [1]
            uint32_t f_ch0_txen: 1;    // [2]
            uint32_t f_ch0_rxhp: 1;    // [3]
            uint32_t reserved_4_7: 4;  // [7:4]
            uint32_t f_ch0_b: 8;       // [15:8]
            uint32_t f_ch0_en: 1;      // [16]
            uint32_t reserved_17_31: 15; // [31:17]
        };
    } ch0_force;          // 0x64

    uint32_t reserved_0x68_0x6c[2]; // 0x68..0x6C

    union {
        uint32_t v;
        struct {
            uint32_t f_rx_on: 1;       // [0]
            uint32_t f_tx_on: 1;       // [1]
            uint32_t reserved_2_31: 30; // [31:2]
        };
    } misc_force;         // 0x70

    uint32_t reserved_0x74; // 0x74

    union {
        uint32_t v;
        struct {
            uint32_t fe_rx_on_del: 12;  // [11:0]
            uint32_t reserved_12_31: 20; // [31:12]
        };
    } fe_rx_del;          // 0x78

    uint32_t reserved_0x7c; // 0x7C

    uint32_t trx_reg[28]; // 0x80..0xEC

    union {
        uint32_t v;
        struct {
            uint32_t rx_avg_q_rd: 12;  // [11:0]
            uint32_t rx_avg_i_rd: 12;  // [23:12]
            uint32_t reserved_24_29: 6; // [29:24]
            uint32_t rx_avg_mode: 1;   // [30]
            uint32_t rx_dc_cal_en: 1;  // [31]
        };
    } rx_avg_calc;        // 0xF0

    uint32_t reserved_0xf4; // 0xF4

    union {
        uint32_t v;
        struct {
            uint32_t rx_dc_q_rd: 12;   // [11:0]
            uint32_t rx_dc_i_rd: 12;   // [23:12]
            uint32_t reserved_24_29: 6; // [29:24]
            uint32_t rx_comp_en: 1;    // [30]
            uint32_t rx_cal_en: 1;     // [31]
        };
    } rx_calib_en;        // 0xF8

    union {
        uint32_t v;
        struct {
            uint32_t rx_phase_err_rd: 10; // [9:0]
            uint32_t reserved_10_15: 6;   // [15:10]
            uint32_t rx_amp_err_rd: 10;   // [25:16]
            uint32_t reserved_26_31: 6;   // [31:26]
        };
    } rx_error_rd;        // 0xFC

    union {
        uint32_t v;
        struct {
            uint32_t rx_ty2_rd: 10;     // [9:0]
            uint32_t reserved_10_31: 22; // [31:10]
        };
    } rx_ty2_rd;          // 0x100

    union {
        uint32_t v;
        struct {
            uint32_t rx_dc_q_wr: 12;   // [11:0]
            uint32_t rx_dc_i_wr: 12;   // [23:12]
            uint32_t reserved_24_31: 8; // [31:24]
        };
    } rx_dc_wr;           // 0x104

    union {
        uint32_t v;
        struct {
            uint32_t rx_phase_err_wr: 10; // [9:0]
            uint32_t reserved_10_15: 6;   // [15:10]
            uint32_t rx_amp_err_wr: 10;   // [25:16]
            uint32_t reserved_26_31: 6;   // [31:26]
        };
    } rx_error_wr;        // 0x108

    uint32_t reserved_0x10c_0x12c[9]; // 0x10C..0x12C

    union {
        uint32_t v;
        struct {
            uint32_t q_const: 10;       // [9:0]
            uint32_t reserved_10_15: 6; // [15:10]
            uint32_t i_const: 10;       // [25:16]
            uint32_t reserved_26_29: 4; // [29:26]
            uint32_t test_pattern: 2;   // [31:30]
        };
    } tx_mode_cfg;        // 0x130

    union {
        uint32_t v;
        struct {
            uint32_t reserved_0_11: 12;  // [11:0]
            uint32_t tx_sin_amp: 4;      // [15:12]
            uint32_t tx_sin_mode: 2;     // [17:16]
            uint32_t reserved_18_21: 4;  // [21:18]
            uint32_t tx_sin_f: 10;       // [31:22]
        };
    } tx_sin_cfg;         // 0x134

    uint32_t reserved_0x138; // 0x138

    union {
        uint32_t v;
        struct {
            uint32_t tx_q_dc_comp: 10;  // [9:0]
            uint32_t reserved_10_15: 6; // [15:10]
            uint32_t tx_i_dc_comp: 10;  // [25:16]
            uint32_t reserved_26_31: 6; // [31:26]
        };
    } tx_dc_comp;         // 0x13C

    union {
        uint32_t v;
        struct {
            uint32_t tx_q_gain_comp: 10; // [9:0]
            uint32_t reserved_10_15: 6;  // [15:10]
            uint32_t tx_i_gain_comp: 10; // [25:16]
            uint32_t reserved_26_31: 6;  // [31:26]
        };
    } tx_gain_comp;       // 0x140

    union {
        uint32_t v;
        struct {
            uint32_t tx_ty2_comp: 10;   // [9:0]
            uint32_t reserved_10_15: 6; // [15:10]
            uint32_t tx_phase_comp: 10; // [25:16]
            uint32_t reserved_26_31: 6; // [31:26]
        };
    } tx_phase_ty2_comp;  // 0x144

    union {
        uint32_t v;
        struct {
            uint32_t reserved_0_5: 6;          // [5:0]
            uint32_t iq_constant_iqcal_p: 10;  // [15:6]
            uint32_t tx_pre_gain: 4;            // [19:16]
            uint32_t reserved_20_20: 1;         // [20]
            uint32_t iq_constant_pout: 10;      // [30:21]
            uint32_t tx_iq_swap: 1;             // [31]
        };
    } tx_other_cfg;       // 0x148

    uint32_t reserved_0x14c; // 0x14C

    union {
        uint32_t v;
        struct {
            uint32_t st_rx_adc_iq: 1;   // [0]
            uint32_t tssi_pout_th: 8;   // [8:1]
            uint32_t dsel_va: 1;        // [9]
            uint32_t tx_dc_n: 2;        // [11:10]
            uint32_t st_rx_adc: 2;      // [13:12]
            uint32_t st_sar_adc: 2;     // [15:14]
            uint32_t tssi_rd: 8;        // [23:16]
            uint32_t agc_pga_rd: 4;     // [27:24]
            uint32_t agc_buf_rd: 1;     // [28]
            uint32_t agc_lna_rd: 2;     // [30:29]
            uint32_t agc_map_mode: 1;   // [31]
        };
    } agc_cfg;            // 0x150

    uint32_t reserved_0x154_0x16c[7]; // 0x154..0x16C

    union {
        uint32_t v;
        struct {
            uint32_t trx_spi_intlv: 10; // [9:0]
            uint32_t reserved_10_30: 21; // [30:10]
            uint32_t trx_bank_rpt: 1;   // [31]
        };
    } trx_spi_intlv;      // 0x170

    uint32_t reserved_0x174_0x18c[7]; // 0x174..0x18C

    uint32_t adda_reg[6]; // 0x190..0x1A4

    union {
        uint32_t v;
        struct {
            uint32_t adda_reg_stat: 6;  // [5:0]
            uint32_t reserved_6_31: 26; // [31:6]
        };
    } adda_reg_stat;      // 0x1A8
} hw_rc_t;

static_assert(sizeof(hw_rc_t) == 428, "");

#define hw_rc ((volatile hw_rc_t *)RC_BASE_ADDR)

#define RC_TRX_REG28_ADDR 0x08628078

inline void rc_trx_reg28_set(uint32_t value) { *((volatile uint32_t *)RC_TRX_REG28_ADDR) = value; }
