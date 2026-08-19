#pragma once

#include <assert.h>

#include "platform/soc.h"


#define RC_BASE_ADDR (0x01050000)


// TRX analog front-end enable map. Three registers share this layout; the SDK programs
// all three whenever a front-end setting changes. Which copy the RC state machine
// selects per RX/TX phase is not documented (NOT INVESTIGATED) — see hw_rc_trx_t.
typedef volatile union {
    uint32_t v;
    struct {
        uint32_t dac_selection: 1;   // [0]
        uint32_t lpf_rx_bw: 1;       // [1]     RX LPF bandwidth: 1 = 40 MHz mode
        uint32_t lpf_tx_bw: 1;       // [2]     TX LPF bandwidth: 1 = 40 MHz mode
        uint32_t lpf_trx_sw: 1;      // [3]
        uint32_t en_lpf: 1;          // [4]
        uint32_t en_if: 1;           // [5]
        uint32_t en_dcoc: 1;         // [6]
        uint32_t en_rx_adc: 1;       // [7]
        uint32_t en_tx_dac: 1;       // [8]
        uint32_t en_tx_dac_bias: 1;  // [9]
        uint32_t en_rx_rssi: 1;      // [10]
        uint32_t en_rx_ref: 1;       // [11]
        uint32_t en_rx_i2v: 1;       // [12]
        uint32_t en_rx_mix: 1;       // [13]
        uint32_t en_lna_cal: 1;      // [14]
        uint32_t en_lna: 1;          // [15]
        uint32_t tx_vin_sel: 1;      // [16]
        uint32_t en_tssi: 1;         // [17]
        uint32_t en_tssi_adc: 1;     // [18]
        uint32_t en_tx_fe_ref: 1;    // [19]
        uint32_t en_tx_fe_bias: 1;   // [20]
        uint32_t en_tx_v2i: 1;       // [21]
        uint32_t en_tx_lo: 1;        // [22]
        uint32_t en_tx_pga: 1;       // [23]
        uint32_t en_pa: 1;           // [24]
        uint32_t en_rx_sw: 1;        // [25]
        uint32_t en_tx_sw: 1;        // [26]
        uint32_t trsw_pll: 1;        // [27]
        uint32_t en_rf_pll: 1;       // [28]
        uint32_t en_doubler: 1;      // [29]    SDK spells this "endobuler"
        uint32_t en_dpll: 1;         // [30]
        uint32_t en_xtal: 1;         // [31]
    };
} hw_rc_trx_en_t;

static_assert(sizeof(hw_rc_trx_en_t) == 4, "");

// BK7011 RF transceiver register bank, mapped at RC_BASE_ADDR + 0x80. Writes go out over
// the RC block's SPI shim, so a write is only complete once the matching bit in
// beken_spi.trx_reg_stat clears — that status field is 28 bits wide, which is what bounds
// this bank. The vendor's REG0x1C ("trx reg28") is not part of it: its table entry aliases
// the RC block's own 0xF0 and no vendor code ever writes it there; the real register is
// reached through the separate address in rc_trx_reg28_set().
//
// Vendor field names carry a trailing bit-range suffix denoting the analog signal's own
// width (isrxlna30 = isrxlna[3:0], vcmsel10 = vcmsel[1:0]); those suffixes are dropped here.
//
// The vendor's live copy of this bank carries no field documentation at all, so every
// description below is inferred from how the calibration code uses the field. Those with no
// corroborating use anywhere in the SDK are marked (NOT INVESTIGATED).
typedef volatile struct {
    union {
        uint32_t v;
        struct {
            uint32_t tssi_iref: 2;        // [1:0]   SDK spells this "TSSliref" (l for I)
            uint32_t g_tssi: 2;           // [3:2]
            uint32_t stb_tssi: 2;         // [5:4]
            uint32_t pcal_att: 3;         // [8:6]   power-calibration attenuation (NOT INVESTIGATED)
            uint32_t en_pcal: 1;          // [9]     enable power calibration (NOT INVESTIGATED)
            uint32_t en_iq_cal: 1;        // [10]
            uint32_t rst_tssi: 1;         // [11]
            uint32_t en_dc_cal: 1;        // [12]
            uint32_t tssi_sel: 1;         // [13]
            uint32_t en_tssi_tst: 1;      // [14]
            uint32_t vsel_tssi: 2;        // [16:15]
            uint32_t tssi_state_ctrl: 1;  // [17]
            uint32_t tssi_sel_range: 1;   // [18]
            uint32_t reserved_19_31: 13;  // [31:19]
        };
    } tssi_ctrl;          // 0x80  (vendor REG0x0)

    union {
        uint32_t v;
        struct {
            uint32_t cp_sel_tx: 2;   // [1:0]   TX charge-pump select (NOT INVESTIGATED)
            uint32_t cp3_tx: 1;      // [2]
            uint32_t cp2_tx: 2;      // [4:3]
            uint32_t cp1_tx: 2;      // [6:5]
            uint32_t ictrl_rx: 4;    // [10:7]
            uint32_t cp_sel_rx: 2;   // [12:11]
            uint32_t rp3_rx: 3;      // [15:13]
            uint32_t rp2_rx: 3;      // [18:16]
            uint32_t lpf_rz_rx: 4;   // [22:19] RX loop-filter zero resistor (NOT INVESTIGATED)
            uint32_t ioffset_rx: 4;  // [26:23]
            uint32_t cp3_rx: 1;      // [27]
            uint32_t cp2_rx: 2;      // [29:28]
            uint32_t cp1_rx: 2;      // [31:30]
        };
    } pll_cp_tx_lpf_rx;   // 0x84  (vendor REG0x1)

    union {
        uint32_t v;
        struct {
            uint32_t vco_amp_ctrl: 1;    // [0]
            uint32_t vref_l_ctrl: 2;     // [2:1]
            uint32_t vref_h_ctrl: 2;     // [4:3]
            uint32_t reset_n_counter: 1; // [5]
            uint32_t reset_n_load: 1;    // [6]
            uint32_t tristate: 1;        // [7]
            uint32_t sel_pol: 1;         // [8]
            uint32_t cp_sw_en: 1;        // [9]
            uint32_t reserved_10_12: 3;  // [12:10]
            uint32_t open: 1;            // [13]    open-loop mode (NOT INVESTIGATED)
            uint32_t ictrl_tx: 4;        // [17:14]
            uint32_t rp3_tx: 3;          // [20:18]
            uint32_t rp2_tx: 3;          // [23:21]
            uint32_t lpf_rz_tx: 4;       // [27:24] TX loop-filter zero resistor (NOT INVESTIGATED)
            uint32_t ioffset_tx: 4;      // [31:28]
        };
    } pll_ctrl_lpf_tx;    // 0x88  (vendor REG0x2)

    union {
        uint32_t v;
        struct {
            uint32_t vnwl: 2;            // [1:0]
            uint32_t vnwh: 2;            // [3:2]
            uint32_t rvco: 3;            // [6:4]
            uint32_t nwresc: 2;          // [8:7]
            uint32_t nwlpf_en: 1;        // [9]
            uint32_t spi_trigger: 1;     // [10]
            uint32_t errdet_spi_en: 1;   // [11]
            uint32_t ckref_loop_sel: 1;  // [12]
            uint32_t ckref_nl_sel: 1;    // [13]
            uint32_t nrst_en: 1;         // [14]
            uint32_t int_mod: 1;         // [15]    integer-N mode, vs fractional SDM (NOT INVESTIGATED)
            uint32_t manual: 1;          // [16]    the SDK writes bandm without ever touching this (NOT INVESTIGATED)
            uint32_t bandm: 7;           // [23:17] manual VCO band
            uint32_t n_manual: 8;        // [31:24] manual N-divider value
        };
    } pll_band_ctrl;      // 0x8C  (vendor REG0x3)

    union {
        uint32_t v;
        struct {
            uint32_t nsdm_lsb: 8;       // [7:0]   sigma-delta modulator fractional LSBs
            uint32_t pn_en: 1;          // [8]
            uint32_t vbnc_div3g: 2;     // [10:9]
            uint32_t itune_div3g: 1;    // [11]
            uint32_t vbnc_div6g: 2;     // [13:12]
            uint32_t itune_div6g: 1;    // [14]
            uint32_t reserved_15_19: 5; // [19:15]
            uint32_t mixer_gm_ctrl: 3;  // [22:20]
            uint32_t pwd_mix_ampdet: 1; // [23]    power down the mixer amplitude detector
            uint32_t mixer_ampdetc: 1;  // [24]
            uint32_t mixer_itune: 1;    // [25]
            uint32_t mixer_ctune: 6;    // [31:26]
        };
    } pll_div_mixer;      // 0x90  (vendor REG0x4)

    union {
        uint32_t v;
        struct {
            uint32_t men_db: 1;         // [0]
            uint32_t flag_polsel_db: 1; // [1]
            uint32_t en_db: 1;          // [2]
            uint32_t dlym_db: 5;        // [7:3]
            uint32_t calen_db: 1;       // [8]
            uint32_t cal_trig_db: 1;    // [9]
            uint32_t reserved_10: 1;    // [10]
            uint32_t cp_ldo: 2;         // [12:11]
            uint32_t pll_ldo: 2;        // [14:13]
            uint32_t vco_ldo: 2;        // [16:15]
            uint32_t cp_rst: 1;         // [17]
            uint32_t sdm_rstn: 1;       // [18]
            uint32_t cksel_sdm: 2;      // [20:19]
            uint32_t cksel_bcal: 1;     // [21]
            uint32_t refsel: 1;         // [22]
            uint32_t pwd_vco: 1;        // [23]
            uint32_t pwd_pll: 1;        // [24]
            uint32_t chspi: 7;          // [31:25] channel frequency offset in MHz above 2400
        };
    } pll_ldo_chan;       // 0x94  (vendor REG0x5)

    union {
        uint32_t v;
        struct {
            uint32_t vcm_sel: 2;      // [1:0]
            uint32_t capcal_sel: 1;   // [2]
            uint32_t dpd_en: 1;       // [3]
            uint32_t lpf_capcal_q: 6; // [9:4]   Q-path LPF capacitor calibration
            uint32_t lpf_capcal_i: 6; // [15:10] I-path LPF capacitor calibration
            uint32_t dcoc_q: 8;       // [23:16] Q-path DC offset compensation
            uint32_t dcoc_i: 8;       // [31:24] I-path DC offset compensation
        };
    } lpf_capcal_dcoc;    // 0x98  (vendor REG0x6)

    union {
        uint32_t v;
        struct {
            uint32_t dig_dcoen: 1;       // [0]     digital DC offset compensation enable
            uint32_t spi_lpf_rx_g: 4;    // [4:1]   RX LPF gain when auto_rx_ifgen is clear
            uint32_t auto_rx_ifgen: 1;   // [5]     let the AGC drive the RX IF gain
            uint32_t dcoc_ctl: 2;        // [7:6]
            uint32_t lpf_vcmic: 2;       // [9:8]
            uint32_t lpf_tx_test: 1;     // [10]
            uint32_t reserved_11_31: 21; // [31:11]
        };
    } lpf_rx_cfg;         // 0x9C  (vendor REG0x7)

    union {
        uint32_t v;
        struct {
            uint32_t is_rx_ref: 2;       // [1:0]
            uint32_t is_rx_lna: 4;       // [5:2]
            uint32_t reserved_6_8: 3;    // [8:6]
            uint32_t is_rx_lnac: 2;      // [10:9]
            uint32_t is_rx_div: 2;       // [12:11]
            uint32_t reserved_13: 1;     // [13]
            uint32_t rssi_th: 6;         // [19:14] RSSI comparator threshold (NOT INVESTIGATED)
            uint32_t is_rssi_gm: 2;      // [21:20]
            uint32_t rssi_ten: 1;        // [22]
            uint32_t reserved_23_31: 9;  // [31:23]
        };
    } rx_bias_rssi;       // 0xA0  (vendor REG0x8)

    union {
        uint32_t v;
        struct {
            uint32_t agc_rxfe_en: 1;     // [0]     let the AGC drive the RX front-end gains
            uint32_t g_rx_lna_spi: 2;    // [2:1]   manual LNA gain
            uint32_t g_rx_i2v_spi: 2;    // [4:3]   manual I2V gain
            uint32_t vs_rx_lna_ldo: 2;   // [6:5]
            uint32_t g_rx_lna_cali: 3;   // [9:7]
            uint32_t vs_rx_mix_ldo: 2;   // [11:10]
            uint32_t vbs_rx_lo: 3;       // [14:12]
            uint32_t vres_rx_lo: 1;      // [15]
            uint32_t ibs_rx_i2v: 2;      // [17:16]
            uint32_t vcms_rx_i2v: 1;     // [18]
            uint32_t reserved_19_31: 13; // [31:19]
        };
    } rx_fe_gain;         // 0xA4  (vendor REG0x9)

    union {
        uint32_t v;
        struct {
            uint32_t dis_ref_pa: 2;     // [1:0]
            uint32_t dis_ref_pga: 2;    // [3:2]
            uint32_t db_pab: 4;         // [7:4]
            uint32_t db_paa: 6;         // [13:8]
            uint32_t reserved_14_15: 2; // [15:14]
            uint32_t dcap_buf: 4;       // [19:16]
            uint32_t db_buf: 4;         // [23:20]
            uint32_t dbc_buf: 4;        // [27:24]
            uint32_t dbc_pa: 3;         // [30:28]
            uint32_t pa_map_en: 1;      // [31]    drive PA level from hw_rc->pa_level_map
        };
    } tx_pa_bias;         // 0xA8  (vendor REG0xA)

    union {
        uint32_t v;
        struct {
            uint32_t dc_pga: 4;    // [3:0]
            uint32_t db_pga: 4;    // [7:4]
            uint32_t dcap_pga: 4;  // [11:8]
            uint32_t dcor_mod: 4;  // [15:12]
            uint32_t dv_lo: 4;     // [19:16]
            uint32_t dvcm_o: 4;    // [23:20]
            uint32_t dcap_mod: 4;  // [27:24]
            uint32_t gctrl_mod: 4; // [31:28]
        };
    } tx_gain_mod;        // 0xAC  (vendor REG0xB)

    union {
        uint32_t v;
        struct {
            uint32_t dgain_pga: 4; // [3:0]
            uint32_t dgain_buf: 4; // [7:4]
            uint32_t dgain_pa: 4;  // [11:8]
            uint32_t dcor_pa: 4;   // [15:12]
            uint32_t vreg_sel: 4;  // [19:16]
            uint32_t lo_vbnc: 2;   // [21:20]
            uint32_t lo_isel: 2;   // [23:22]
            uint32_t dvbb: 4;      // [27:24]
            uint32_t dtr: 4;       // [31:28]
        };
    } tx_gain_pa;         // 0xB0  (vendor REG0xC)

    hw_rc_trx_en_t en_map0; // 0xB4  (vendor REG0xD)
    hw_rc_trx_en_t en_map1; // 0xB8  (vendor REG0xE)

    union {
        uint32_t v;
        struct {
            uint32_t reg_bank_sel: 1;    // [0]
            uint32_t clk_dac_sel: 1;     // [1]     1 = 40 MHz mode
            uint32_t clk_adc_sel: 1;     // [2]     1 = 40 MHz mode
            uint32_t reserved_3_4: 2;    // [4:3]
            uint32_t sinad_tx_en: 1;     // [5]
            uint32_t sinad_rx_en: 1;     // [6]
            uint32_t tssi_cal_en: 1;     // [7]     gates the validity of agc_cfg.tssi_rd
            uint32_t reserved_8_12: 5;   // [12:8]
            uint32_t sinad_hpf_coef: 2;  // [14:13]
            uint32_t clk_dac_inv: 1;     // [15]
            uint32_t clk_adc_inv: 1;     // [16]
            uint32_t reserved_17_27: 11; // [27:17]
            uint32_t sys_ldo_en: 1;      // [28]
            uint32_t sys_ldo_lp: 1;      // [29]
            uint32_t en_dsp_vdd: 1;      // [30]
            uint32_t dig_ldo_lp: 1;      // [31]
        };
    } sys_ctrl;           // 0xBC  (vendor REG0xF)

    hw_rc_trx_en_t en_map2; // 0xC0  (vendor REG0x10)

    union {
        uint32_t v;
        struct {
            uint32_t tx_sinad_table: 11; // [10:0]
            uint32_t reserved_11_31: 21; // [31:11]
        };
    } tx_sinad;           // 0xC4  (vendor REG0x11)

    union {
        uint32_t v;
        struct {
            uint32_t lpf_out_tst_en: 1;    // [0]
            uint32_t lpf_in_tst_en: 1;     // [1]
            uint32_t if_buf_ic: 3;         // [4:2]
            uint32_t adc_ref_sel: 2;       // [6:5]
            uint32_t ldo_adda: 2;          // [8:7]
            uint32_t buf_tst_selection: 3; // [11:9]
            uint32_t reserved_12: 1;       // [12]
            uint32_t en_tst_buf_ldo: 1;    // [13]
            uint32_t adc_in_sel: 1;        // [14]
            uint32_t en_gm_boost: 1;       // [15]
            uint32_t adc_isel_r: 3;        // [18:16]
            uint32_t adc_isel_c: 3;        // [21:19]
            uint32_t adc_ref_bw_sel: 1;    // [22]   1 = 40 MHz mode
            uint32_t adc_ten: 1;           // [23]
            uint32_t fictrl: 4;            // [27:24]
            uint32_t flvcm: 2;             // [29:28]
            uint32_t ldo_if_sel3v: 2;      // [31:30]
        };
    } adc_test_cfg;       // 0xC8  (vendor REG0x12)

    union {
        uint32_t v;
        struct {
            uint32_t r2_rx: 2;   // [1:0]
            uint32_t c1_rx: 2;   // [3:2]
            uint32_t rz_rx: 5;   // [8:4]
            uint32_t icp_rx: 5;  // [13:9]  RX charge-pump current
            uint32_t icp_off: 3; // [16:14]
            uint32_t icp_sel: 1; // [17]
            uint32_t r2_tx: 2;   // [19:18]
            uint32_t c1_tx: 2;   // [21:20]
            uint32_t rz_tx: 5;   // [26:22]
            uint32_t icp_tx: 5;  // [31:27] TX charge-pump current
        };
    } pll_cp_cfg;         // 0xCC  (vendor REG0x13)

    // RX DC-offset calibration table, one register per two RX gain steps:
    // entry n holds 6*n dB in the _lo pair and 6*n+3 dB in the _hi pair (0..45 dB).
    union {
        uint32_t v;
        struct {
            uint32_t dc_i_lo: 8; // [7:0]
            uint32_t dc_q_lo: 8; // [15:8]
            uint32_t dc_i_hi: 8; // [23:16]
            uint32_t dc_q_hi: 8; // [31:24]
        };
    } rx_dc_cal[8];       // 0xD0..0xEC  (vendor REG0x14..REG0x1B)
} hw_rc_trx_t;

static_assert(sizeof(hw_rc_trx_t) == 112, "");


typedef volatile struct {
    union {
        uint32_t v;
        struct {
            uint32_t ch0_en: 1;          // [0]
            uint32_t reserved_1_2: 2;    // [2:1]
            uint32_t rc_en: 1;           // [3]
            uint32_t reserved_4_7: 4;    // [7:4]
            // BK7011 LD output. Reads 1 while the RFPLL is *not* locked: the vendor band
            // sweep loops `do {...} while (ch0_ld)` and its timer restarts a band
            // recalibration whenever this bit is set.
            uint32_t ch0_ld: 1;          // [8]
            uint32_t lna_rssi: 1;        // [9]     BK7011 LNA RSSI state
            uint32_t reserved_10_11: 2;  // [11:10]
            uint32_t ch0_shdn_stat: 1;   // [12]    BK7011 is in shutdown
            uint32_t reserved_13_15: 3;  // [15:13]
            // 0 = SPI_RESET, 1 = SHUTDOWN, 2 = WAIT_SPI (SPI busy), 3 = WAIT_LOCK (RFPLL locking), 4 = ACTIVE
            uint32_t rc_state: 3;        // [18:16]
            uint32_t reserved_19_29: 11; // [29:19]
            uint32_t spi_reset: 1;       // [30]    reset BK7011's SPI registers
            uint32_t force_enable: 1;    // [31]    force-drive BK7011's interface signals
        };
    } cntl_stat;          // 0x00

    union {
        uint32_t v;
        struct {
            // One bit per hw_rc_trx_t word: 0 = idle, 1 = update in flight (do not write that word again)
            uint32_t trx_reg_stat: 28; // [27:0]
            uint32_t prescaler: 4;     // [31:28] SPI clock = RC clock (80 MHz) / 2 / prescaler
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

    hw_rc_trx_t trx; // 0x80..0xEC

    union {
        uint32_t v;
        struct {
            uint32_t rx_avg_q_rd: 12;   // [11:0]
            uint32_t rx_avg_i_rd: 12;   // [23:12]
            uint32_t reserved_24_27: 4; // [27:24]
            uint32_t rx_hpf_bypass: 1;  // [28]
            uint32_t rx_iq_swap: 1;     // [29]
            uint32_t rx_avg_mode: 1;    // [30]    0 = mean of the signal, 1 = mean of |signal|
            uint32_t rx_dc_cal_en: 1;   // [31]
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
            uint32_t rx_phase_err_rd: 10; // [9:0]   estimated phase error, signed, phase_err_est * 2^9
            uint32_t reserved_10_15: 6;   // [15:10]
            uint32_t rx_amp_err_rd: 10;   // [25:16] estimated amplitude error, signed, amp_err_est * 2^9
            uint32_t reserved_26_31: 6;   // [31:26]
        };
    } rx_error_rd;        // 0xFC

    union {
        uint32_t v;
        struct {
            uint32_t rx_ty2_rd: 10;     // [9:0]   estimated TY2, signed, (ty2 - 0.5) * 2^10
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
            uint32_t q_const: 10;       // [9:0]   Q value transmitted in constant mode
            uint32_t reserved_10_15: 6; // [15:10]
            uint32_t i_const: 10;       // [25:16] I value transmitted in constant mode
            uint32_t dpd_comp_en: 1;    // [26]
            uint32_t dpd_cal_en: 1;     // [27]
            uint32_t dpd_en: 1;         // [28]
            uint32_t tx_comp_dis: 1;    // [29]    0 = TX compensation enabled, 1 = disabled
            uint32_t test_pattern: 2;   // [31:30] 0 = normal, transmit the modem's data
        };
    } tx_mode_cfg;        // 0x130

    union {
        uint32_t v;
        struct {
            uint32_t reserved_0_11: 12;  // [11:0]
            uint32_t tx_sin_amp: 4;      // [15:12] sine amplitude, fixed point x.yyy, range [0, 15/8]
            uint32_t tx_sin_mode: 2;     // [17:16] 0 = complex I/Q, 1 = I only, 2 = Q only, 3 = reserved
            uint32_t reserved_18_21: 4;  // [21:18]
            uint32_t tx_sin_f: 10;       // [31:22] sine frequency: tx_sin_f = 2*pi*F / 80e6 * 2^8
        };
    } tx_sin_cfg;         // 0x134

    union {
        uint32_t v;
        struct {
            uint32_t hbf40_sel: 1;      // [0]     40 MHz upsampling filter coefficients: 1 = Beken, 0 = RW
            uint32_t hbf40_bypass: 1;   // [1]     bypass the 40 MHz upsampling filter
            uint32_t hbf20_sel: 1;      // [2]     20 MHz upsampling filter coefficients: 1 = Beken, 0 = RW
            uint32_t hbf20_bypass: 1;   // [3]     bypass the 20 MHz upsampling filter
            uint32_t reserved_4_31: 28; // [31:4]
        };
    } hbf_cfg;            // 0x138

    union {
        uint32_t v;
        struct {
            uint32_t tx_q_dc_comp: 10;  // [9:0]   Q DC offset, signed, range [-512, 511]
            uint32_t reserved_10_15: 6; // [15:10]
            uint32_t tx_i_dc_comp: 10;  // [25:16] I DC offset, signed, range [-512, 511]
            uint32_t reserved_26_31: 6; // [31:26]
        };
    } tx_dc_comp;         // 0x13C

    union {
        uint32_t v;
        struct {
            uint32_t tx_q_gain_comp: 10; // [9:0]   0 .. 1023/1024, step 1/1024
            uint32_t reserved_10_15: 6;  // [15:10]
            uint32_t tx_i_gain_comp: 10; // [25:16] 0 .. 1023/1024, step 1/1024
            uint32_t reserved_26_31: 6;  // [31:26]
        };
    } tx_gain_comp;       // 0x140

    union {
        uint32_t v;
        struct {
            uint32_t tx_ty2_comp: 10;   // [9:0]   512/1024 .. 1535/1024, step 1/1024
            uint32_t reserved_10_15: 6; // [15:10]
            uint32_t tx_phase_comp: 10; // [25:16] -512/1024 .. 511/1024, step 1/1024
            uint32_t reserved_26_31: 6; // [31:26]
        };
    } tx_phase_ty2_comp;  // 0x144

    union {
        uint32_t v;
        struct {
            uint32_t reserved_0_5: 6;         // [5:0]
            uint32_t iq_constant_iqcal_p: 10; // [15:6]  IQ calibration, positive IQ constant input
            // IQ signal gain: 0 = +1.75 dB descending to 31 = -6 dB. Vendor sources disagree on the
            // width — the cal headers say 5 bits (and the 32-step scale needs them), reg_rc.h says 4
            // with bit 20 undocumented. Vendor power tables only ever load 0..0xF, so usage can't
            // settle it; 5 is used here (NOT TESTED).
            uint32_t tx_pre_gain: 5;          // [20:16]
            uint32_t iq_constant_pout: 10;    // [30:21] power calibration, IQ constant input
            uint32_t tx_iq_swap: 1;           // [31]
        };
    } tx_other_cfg;       // 0x148

    uint32_t reserved_0x14c; // 0x14C

    union {
        uint32_t v;
        struct {
            uint32_t st_rx_adc_iq: 1; // [0]     RX ADC IQ cal settling: 0 = 300 us, 1 = 600 us
            uint32_t tssi_pout_th: 8; // [8:1]   TSSI threshold for the target output power
            uint32_t dsel_va: 1;      // [9]     version-A readout select: 0 = TSSI, 1 = AGC gain setting
            uint32_t tx_dc_n: 2;      // [11:10] TX DC bisection first-pass steps: 0 = 3, 1 = 4, 2 = 5, 3 = 6
            uint32_t st_rx_adc: 2;    // [13:12] RX ADC settling: 0 = 100 us, 1 = 200, 2 = 300, 3 = 400
            uint32_t st_sar_adc: 2;   // [15:14] SAR ADC settling: 0 = 0.5 us, 1 = 1, 2 = 1.5, 3 = 2
            uint32_t tssi_rd: 8;      // [23:16] TSSI value; only valid while tssi_cal_en is asserted
            uint32_t agc_pga_rd: 4;   // [27:24]
            uint32_t agc_buf_rd: 1;   // [28]
            uint32_t agc_lna_rd: 2;   // [30:29]
            uint32_t agc_map_mode: 1; // [31]    AGC gain mapping: 0 = mode 1, 1 = mode 2
        };
    } agc_cfg;            // 0x150

    union {
        uint32_t v;
        struct {
            uint32_t rx_snr_noise: 9;   // [8:0]   RX noise level for the SINAD test
            uint32_t reserved_9_15: 7;  // [15:9]
            uint32_t rx_snr_sig: 9;     // [24:16] RX signal level for the SINAD test
            uint32_t reserved_25_31: 7; // [31:25]
        };
    } rx_sinad_rd;        // 0x154

    uint32_t reserved_0x158_0x164[4]; // 0x158..0x164

    union {
        uint32_t v;
        struct {
            uint32_t tx_cal_cap_i: 6;      // [5:0]   I-path calibration capacitance, transmit mode
            uint32_t reserved_6_7: 2;      // [7:6]
            uint32_t rx_cal_cap_i: 6;      // [13:8]  I-path calibration capacitance, receive mode
            uint32_t reserved_14_15: 2;    // [15:14]
            uint32_t standby_cal_cap_i: 6; // [21:16] I-path calibration capacitance, standby mode
            uint32_t reserved_22_31: 10;   // [31:22]
        };
    } cal_cap_i;          // 0x168

    union {
        uint32_t v;
        struct {
            uint32_t tx_cal_cap_q: 6;      // [5:0]   Q-path calibration capacitance, transmit mode
            uint32_t reserved_6_7: 2;      // [7:6]
            uint32_t rx_cal_cap_q: 6;      // [13:8]  Q-path calibration capacitance, receive mode
            uint32_t reserved_14_15: 2;    // [15:14]
            uint32_t standby_cal_cap_q: 6; // [21:16] Q-path calibration capacitance, standby mode
            uint32_t reserved_22_31: 10;   // [31:22]
        };
    } cal_cap_q;          // 0x16C

    union {
        uint32_t v;
        struct {
            uint32_t trx_spi_intlv: 10;  // [9:0]   TRX SPI transfer interval, in 12.5 ns units
            uint32_t reserved_10_30: 21; // [30:10]
            uint32_t trx_bank_rpt: 1;    // [31]    TRX bank report: 0 = bank0, 1 = bank1
        };
    } trx_spi_intlv;      // 0x170

    uint32_t reserved_0x174_0x18c[7]; // 0x174..0x18C

    uint32_t adda_reg[6]; // 0x190..0x1A4

    union {
        uint32_t v;
        struct {
            // One bit per adda_reg[n]: 0 = idle, 1 = update in flight (do not write that register again)
            uint32_t adda_reg_stat: 6;  // [5:0]
            uint32_t reserved_6_31: 26; // [31:6]
        };
    } adda_reg_stat;      // 0x1A8

    uint32_t reserved_0x1ac_0x1bc[5]; // 0x1AC..0x1BC

    // PA output-power level mapping table: 16 levels packed two per register,
    // even level in [15:0], odd level in [31:16]. Applied when trx.tx_pa_bias.pa_map_en is set.
    union {
        uint32_t v;
        struct {
            uint32_t level_even: 16; // [15:0]  level 2*n
            uint32_t level_odd: 16;  // [31:16] level 2*n + 1
        };
    } pa_level_map[8];    // 0x1C0..0x1DC
} hw_rc_t;

static_assert(sizeof(hw_rc_t) == 480, "");

#define hw_rc ((volatile hw_rc_t *)RC_BASE_ADDR)

#define RC_TRX_REG28_ADDR 0x08628078

inline void rc_trx_reg28_set(uint32_t value) { *((volatile uint32_t *)RC_TRX_REG28_ADDR) = value; }
