#pragma once

#include "register_defs.h"


#define SCTRL_BASE                  (0x00800000)


typedef volatile struct {
    uint32_t chip_id;
    uint32_t device_id;

    union {
        uint32_t v;
        struct _register_bits {
            uint32_t mclk_source: 2;            // [0:1]
            uint32_t hclk_div2: 2;              // [2:3]
            uint32_t divider: 4;                // [4:7]
            uint32_t flash_26m: 1;              // [8]
            uint32_t hclk_div2_en: 1;           // [9]
            uint32_t modem_clk480m_pwd: 1;      // [10]
            uint32_t mac_clk480m_pwd: 1;        // [11]
            uint32_t mpif_clk_invert_bit: 1;    // [12]
            uint32_t sdio_clk_invert_bit: 1;    // [13]
            uint32_t dpll_div_reset: 1;         // [14]
            uint32_t ble_rf_en: 1;              // [15]
            uint32_t qspi_io_volt: 2;           // [16:17]
            uint32_t flash_sck_io_cap: 2;       // [18:19]
            uint32_t psram_vddpad_volt: 2;      // [20:21]
            uint32_t flash_spi_mux_bit: 1;      // [22]
            uint32_t efuse_vdd25_en: 1;         // [23]
            uint32_t reserved_24_31: 8;         // [24:31]
        };
    } control;

    union {
        uint32_t v;
        struct _register_bits {
            uint32_t mac_mpif: 1;               // [0]
            uint32_t mac_wt: 1;                 // [1]
            uint32_t mac_core_rx: 1;            // [2]
            uint32_t mac_core_tx: 1;            // [3]
            uint32_t mac_crypt: 1;              // [4]
            uint32_t mac_pri: 1;                // [5]
            uint32_t mac_pi_tx: 1;              // [6]
            uint32_t mac_pi_rx: 1;              // [7]
            uint32_t mac_pi: 1;                 // [8]
            uint32_t reserved_9_31: 23;         // [9:31]
        };
    } clk_gating;

    union {
        uint32_t v;
        struct _register_bits {
            uint32_t modem_subchip_reset: 1;    // [0]
            uint32_t dsp_subsys_reset: 1;       // [1]
            uint32_t mac_subsys_reset: 1;       // [2]
            uint32_t tl410_boot: 1;             // [3]
            uint32_t usb_subsys_reset: 1;       // [4]
            uint32_t tl410_ext_wait: 1;         // [5]
            uint32_t modem_core_reset: 1;       // [6]
            uint32_t mac_wakeup_arm: 1;         // [7]
            uint32_t ble_subsys_reset: 1;       // [8]
            uint32_t reserved_9_31: 23;         // [9:31]
        };
    } reset;

    uint32_t reserved_5;
    uint32_t reserved_6;

    union {
        uint32_t v;
        struct _register_bits {
            uint32_t cycle_value: 12;           // [0:11]
            uint32_t reserved_12_31: 20;        // [12:31]
        };
    } flash_delay;

    uint32_t modem_subchip_reset_request;
    uint32_t mac_subsys_reset_request;
    uint32_t usb_subsys_reset_request;
    uint32_t dsp_subsys_reset_request;

    union {
        uint32_t v;
        struct _register_bits {
            uint32_t phy_hclk_enable: 1;        // [0]
            uint32_t mac_hclk_enable: 1;        // [1]
            uint32_t reset_request: 16;         // [2:17]
            uint32_t reserved_18_31: 14;        // [18:31]
        };
    } modem_core_reset_phy_hclk;

    union {
        uint32_t v;
        struct _register_bits {
            uint32_t reserved_0: 1;             // [0]
            uint32_t reserved_1: 1;             // [1]
            uint32_t dbg_mdm_bank0_mux: 5;      // [2:6]
            uint32_t reserved_7: 1;             // [7]
            uint32_t dbg_mdm_bank1_mux: 5;      // [8:12]
            uint32_t reserved_13_15: 3;         // [13:15]
            uint32_t smp_source: 4;             // [16:19]
            uint32_t dbg_mac_clk_sel: 1;        // [20]
            uint32_t reserved_21_31: 11;        // [21:31]
        };
    } debug13;

    union {
        uint32_t v;
    } debug14_modem;

    union {
        uint32_t v;
    } debug15_mac;

    uint32_t analog_spi;
    uint32_t reserved_17;
    uint32_t la_sample;
    uint32_t la_sample_value;
    uint32_t la_sample_mask;
    uint32_t la_sample_dma_start_addr;
    uint32_t analog_ctrl0;
    uint32_t analog_ctrl1;
    uint32_t analog_ctrl2;
    uint32_t analog_ctrl3;
    uint32_t analog_ctrl4;
    uint32_t analog_ctrl5;
    uint32_t analog_ctrl6;
    uint32_t efuse_ctrl;
    uint32_t efuse_optr;
    uint32_t reserved_31;
    uint32_t reserved_32;
    uint32_t charge_status;
    uint32_t analog_ctrl7;
    uint32_t analog_ctrl8;
    uint32_t analog_ctrl9;
    uint32_t analog_ctrl10;
    uint32_t reserved_38;
    uint32_t reserved_39;
    uint32_t reserved_40;
    uint32_t reserved_41;
    uint32_t reserved_42;
    uint32_t reserved_43;
    uint32_t reserved_44;
    uint32_t reserved_45;
    uint32_t reserved_46;
    uint32_t reserved_47;
    uint32_t reserved_48;
    uint32_t reserved_49;
    uint32_t reserved_50;
    uint32_t reserved_51;
    uint32_t reserved_52;
    uint32_t reserved_53;
    uint32_t reserved_54;
    uint32_t reserved_55;
    uint32_t reserved_56;
    uint32_t reserved_57;
    uint32_t reserved_58;
    uint32_t reserved_59;
    uint32_t reserved_60;
    uint32_t reserved_61;
    uint32_t reserved_62;
    uint32_t reserved_63;
    uint32_t low_power_clk;
    uint32_t sleep;
    uint32_t digital_vdd;
    uint32_t power_mac_modem;
    uint32_t power_dsp;
    uint32_t power_usb;
    uint32_t reserved_70;
    uint32_t reserved_71;
    uint32_t reserved_72;
    uint32_t reserved_73;
    uint32_t reserved_74;
    union {
        uint32_t v;
        struct _register_bits {
            uint32_t flash: 1;                  // [0]
            uint32_t dco: 1;                    // [1]
            uint32_t rosc_32k: 1;               // [2]
            uint32_t xtal_26m: 1;               // [3]
            uint32_t xtal_32k: 1;               // [4]
            uint32_t dpll_480m: 1;              // [5]
            uint32_t digital_core: 1;           // [6]
            uint32_t digital_core_ldo_low_power: 1;// [7]
            uint32_t analog_sys_ldo: 1;         // [8]
            uint32_t io_ldo_low_power: 1;       // [9]
            uint32_t xtal_2_rf: 1;              // [10]
            uint32_t xtal_26m_low_power: 1;     // [11]
            uint32_t temprature_sensor: 1;      // [12]
            uint32_t saradc: 1;                 // [13]
            uint32_t usb: 1;                    // [14]
            uint32_t audio_random_generator: 1; // [15]
            uint32_t audio_pll: 1;              // [16]
            uint32_t audio: 1;                  // [17]
            uint32_t mic_qspi_ram_or_flash: 1;  // [18]
            uint32_t nc: 1;                     // [19]
            uint32_t write_key: 12;             // [20:31] set 0xA5C to write
        };
    } block_enable;
    uint32_t reserved_76;
    uint32_t reserved_77;
    uint32_t reserved_78;
    uint32_t reserved_79;
    uint32_t reserved_80;
    uint32_t reserved_81;
    uint32_t reserved_82;
    uint32_t reserved_83;
    uint32_t reserved_84;

} hw_sctrl_t;

#define hw_sctrl                    ((volatile hw_sctrl_t *) SCTRL_BASE)


#define MODEM_SUBCHIP_RESET_WORD    (0x7111e802)
#define MAC_SUBSYS_RESET_WORD       (0x7111c802)
#define USB_SUBSYS_RESET_WORD       (0x7111c12B)
#define DSP_SUBSYS_RESET_WORD       (0x7111c410)
#define MODEM_CORE_RESET_WORD       (0xE802)


#define SCTRL_BLOCK_ENABLE_WRITE_KEY    (0xA5C)


static inline void sctrl_block_enable_temprature_sensor() {
    typeof(hw_sctrl->block_enable) tmp;
    tmp.v = hw_sctrl->block_enable.v;
    tmp.temprature_sensor = 1;
    tmp.write_key = SCTRL_BLOCK_ENABLE_WRITE_KEY;
    hw_sctrl->block_enable.v = tmp.v;
}

static inline void sctrl_block_disable_temprature_sensor() {
    typeof(hw_sctrl->block_enable) tmp;
    tmp.v = hw_sctrl->block_enable.v;
    tmp.temprature_sensor = 0;
    tmp.write_key = SCTRL_BLOCK_ENABLE_WRITE_KEY;
    hw_sctrl->block_enable.v = tmp.v;
}
