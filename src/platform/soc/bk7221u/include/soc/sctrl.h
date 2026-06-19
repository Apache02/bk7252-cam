#pragma once

#include <assert.h>
#include "platform/soc.h"

#define SCTRL_BASE                  (0x00800000)


typedef volatile struct {
    uint32_t chip_id;                              // [+0x00] chip identifier; reads 0x7221a on BK7252
    uint32_t device_id;                            // [+0x04] silicon revision identifier

    union {
        uint32_t v;
        struct {
            uint32_t mclk_source: 2;              // [1:0]   0=DCO, 1=26M_XTAL, 2=DPLL, 3=LPO
            uint32_t reserved_2_3: 2;             // [3:2]
            uint32_t divider: 4;                  // [7:4]   MCU clock = source / (divider+1) when DPLL
            uint32_t flash_26m: 1;                // [8]     1 = flash clock uses 26 MHz XTAL
            uint32_t hclk_div2_en: 1;             // [9]     1 = AHB bus clock halved from MCU clock
            uint32_t modem_clk480m_pwd: 1;        // [10]    1 = power down modem 480 MHz clock
            uint32_t mac_clk480m_pwd: 1;          // [11]    1 = power down MAC 480 MHz clock
            uint32_t mpif_clk_invert: 1;          // [12]    invert MPIF clock
            uint32_t sdio_clk_invert: 1;          // [13]    invert SDIO clock
            uint32_t dpll_div_reset: 1;           // [14]    DPLL clock-divider reset
            uint32_t ble_rf_en: 1;                // [15]    1 = BLE RF enabled
            uint32_t qspi_io_volt: 2;             // [17:16] QSPI I/O voltage select
            uint32_t flash_sck_io_cap: 2;         // [19:18] flash SCK I/O capacitance
            uint32_t psram_vddpad_volt: 2;        // [21:20] PSRAM VDD pad voltage
            uint32_t flash_spi_mux: 1;            // [22]    1 = flash driven by SPI controller (not flash controller)
            uint32_t efuse_vdd25_en: 1;           // [23]    1 = eFuse 2.5 V supply enabled
            uint32_t reserved_24_31: 8;           // [31:24]
        };
    } control;

    union {
        uint32_t v;
        struct {
            uint32_t mac_mpif: 1;                 // [0]
            uint32_t mac_wt: 1;                   // [1]
            uint32_t mac_core_rx: 1;              // [2]
            uint32_t mac_core_tx: 1;              // [3]
            uint32_t mac_crypt: 1;                // [4]
            uint32_t mac_pri: 1;                  // [5]
            uint32_t mac_pi_tx: 1;                // [6]
            uint32_t mac_pi_rx: 1;                // [7]
            uint32_t mac_pi: 1;                   // [8]
            uint32_t reserved_9_31: 23;           // [31:9]
        };
    } clk_gating;

    union {
        uint32_t v;
        struct {
            uint32_t modem_subchip_reset: 1;      // [0]
            uint32_t dsp_subsys_reset: 1;         // [1]
            uint32_t mac_subsys_reset: 1;         // [2]
            uint32_t tl410_boot: 1;               // [3]
            uint32_t usb_subsys_reset: 1;         // [4]
            uint32_t tl410_ext_wait: 1;           // [5]
            uint32_t modem_core_reset: 1;         // [6]
            uint32_t mac_wakeup_arm: 1;           // [7]
            uint32_t ble_subsys_reset: 1;         // [8]
            uint32_t reserved_9_31: 23;           // [31:9]
        };
    } reset;

    uint32_t reserved_0x14_0x18[2];               // offsets 5-6

    union {
        uint32_t v;
        struct {
            uint32_t cycle_value: 12;             // [11:0]  flash access delay in MCU clock cycles
            uint32_t reserved_12_31: 20;          // [31:12]
        };
    } flash_delay;

    uint32_t modem_subchip_reset_request;         // write MODEM_SUBCHIP_RESET_WORD
    uint32_t mac_subsys_reset_request;            // write MAC_SUBSYS_RESET_WORD
    uint32_t usb_subsys_reset_request;            // write USB_SUBSYS_RESET_WORD
    uint32_t dsp_subsys_reset_request;            // write DSP_SUBSYS_RESET_WORD

    union {
        uint32_t v;
        struct {
            uint32_t phy_hclk_enable: 1;          // [0]     modem AHB clock enable
            uint32_t mac_hclk_enable: 1;          // [1]     MAC AHB clock enable
            uint32_t reserved_2_15: 14;           // [15:2]
            uint32_t reset_word: 16;              // [31:16] write MODEM_CORE_RESET_WORD to trigger modem core reset
        };
    } modem_core_reset_phy_hclk;

    union {
        uint32_t v;
        struct {
            uint32_t reserved_0_1: 2;             // [1:0]
            uint32_t dbg_mdm_bank0_mux: 5;        // [6:2]
            uint32_t reserved_7: 1;               // [7]
            uint32_t dbg_mdm_bank1_mux: 5;        // [12:8]
            uint32_t reserved_13_15: 3;           // [15:13]
            uint32_t smp_source: 4;               // [19:16]
            uint32_t dbg_mac_clk_sel: 1;          // [20]
            uint32_t reserved_21_31: 11;          // [31:21]
        };
    } debug13;

    uint32_t debug14_modem;
    uint32_t debug15_mac;

    union {
        uint32_t v;
        struct {
            uint32_t state: 11;                   // [10:0]  analog SPI state machine; nonzero = busy
            uint32_t reserved_11_25: 15;          // [25:11]
            uint32_t freq_div: 6;                 // [31:26] analog SPI clock divider
        };
    } analog_spi;

    uint32_t reserved_0x44;                       // offset 17

    union {
        uint32_t v;
        struct {
            uint32_t mode: 2;                     // [1:0]   logic-analyser sample mode
            uint32_t clk_invert: 1;               // [2]
            uint32_t finish: 1;                   // [3]     read-only; 1 = capture complete
            uint32_t reserved_4_15: 12;           // [15:4]
            uint32_t length: 16;                  // [31:16] capture depth in samples
        };
    } la_sample;

    uint32_t la_sample_value;
    uint32_t la_sample_mask;
    uint32_t la_sample_dma_start_addr;

    uint32_t analog_ctrl0;                        // DPLL calibration trigger / SPI detect
    uint32_t analog_ctrl1;                        // DCO calibration control
    uint32_t analog_ctrl2;                        // XTAL tune / bias
    uint32_t analog_ctrl3;                        // charge pump / LDO
    uint32_t analog_ctrl4;                        // sys LDO / charge mode

    uint32_t analog_ctrl5;                        // (NOT INVESTIGATED)

    union {
        uint32_t v;
        struct {
            uint32_t reserved_0_2: 3;             // [2:0]
            uint32_t dpll_reset: 1;               // [3]
            uint32_t reserved_4_10: 7;            // [10:4]
            uint32_t rp_ctrl_lpf: 3;              // [13:11] LPF resistor control
            uint32_t vsel_cal: 1;                 // [14]    calibration voltage select
            uint32_t kvco_ctrl: 2;                // [16:15] VCO gain control
            uint32_t clk_ref_loop_sel: 1;         // [17]
            uint32_t spi_trigger: 1;              // [18]
            uint32_t vco_band_manual_en: 1;       // [19]
            uint32_t charge_pump_cur_ctrl: 3;     // [22:20] charge pump current
            uint32_t ref_clk_select: 1;           // [23]
            uint32_t xtal26m_clk_audio_en: 1;     // [24]   route 26 MHz XTAL to audio PLL
            uint32_t clkout_pad_en: 1;            // [25]   DPLL clock output pad enable
            uint32_t divider_ctrl: 3;             // [28:26] output clock divider ratio
            uint32_t divider_clk_sel: 1;          // [29]
            uint32_t clk_for_usb_en: 1;           // [30]   route DPLL to USB
            uint32_t clk_for_audio_en: 1;         // [31]   route DPLL to audio
        };
    } analog_ctrl6;

    // efuse_ctrl and efuse_optr are mapped here (offsets 0x1D-0x1E) but owned by hardware_efuse
    // via hw_efuse (soc/efuse.h); access through hw_sctrl->efuse_ctrl only when bypassing that driver.
    uint32_t efuse_ctrl;
    uint32_t efuse_optr;

    uint32_t dma_prio_val;                        // DMA priority configuration (NOT INVESTIGATED)
    uint32_t ble_subsys_reset_request;            // write 0 to reset BLE subsystem

    union {
        uint32_t v;
        struct {
            uint32_t vcvcal: 5;                   // [4:0]   charger CV calibration value (read-only)
            uint32_t vcal: 6;                     // [10:5]  charger voltage calibration (read-only)
            uint32_t lcal: 5;                     // [15:11] charger current calibration (read-only)
            uint32_t reserved_16_31: 16;          // [31:16]
        };
    } charge_status;

    uint32_t analog_ctrl7;                        // (NOT INVESTIGATED)

    union {
        uint32_t v;
        struct {
            uint32_t dac_mute_en: 1;              // [0]
            uint32_t reserved_1: 1;               // [1]
            uint32_t dac_gain: 5;                 // [6:2]
            uint32_t reserved_7_19: 13;           // [19:7]
            uint32_t adc_r_pwd: 1;                // [20]   1 = power down ADC right channel
            uint32_t adc_l_pwd: 1;                // [21]   1 = power down ADC left channel
            uint32_t line_in_gain: 2;             // [23:22]
            uint32_t line_in_en: 1;               // [24]
            uint32_t reserved_25_31: 7;           // [31:25]
        };
    } analog_ctrl8;

    union {
        uint32_t v;
        struct {
            uint32_t reserved_0_5: 6;             // [5:0]
            uint32_t dga_en: 1;                   // [6]    digital gain amplifier enable
            uint32_t reserved_7_22: 16;           // [22:7]
            uint32_t driver_output_en: 1;         // [23]   DAC driver output enable
            uint32_t pa_output_en: 1;             // [24]   DAC PA output enable
            uint32_t reserved_25_28: 4;           // [28:25]
            uint32_t dac_r_en: 1;                 // [29]   DAC right channel enable
            uint32_t dac_l_en: 1;                 // [30]   DAC left channel enable
            uint32_t diff_en: 1;                  // [31]   differential output enable
        };
    } analog_ctrl9;

    union {
        uint32_t v;
        struct {
            uint32_t reserved_0: 1;               // [0]
            uint32_t vsel: 2;                     // [2:1]  DAC output voltage select
            uint32_t reserved_3_6: 4;             // [6:3]
            uint32_t n_end_oupt_r: 1;             // [7]    DAC N-end output right channel
            uint32_t n_end_oupt_l: 1;             // [8]    DAC N-end output left channel
            uint32_t reserved_9_31: 23;           // [31:9]
        };
    } analog_ctrl10;

    uint32_t reserved_0x98_0xFC[26];              // offsets 38–63

    union {
        uint32_t v;
        struct {
            uint32_t lpo_clk_mux: 2;              // [1:0]  0=ROSC, 1=32K_XTAL, 2=32K_DIV
            uint32_t reserved_2_31: 30;           // [31:2]
        };
    } low_power_clk;

    union {
        uint32_t v;
        struct {
            uint32_t sleep_mode: 16;              // [15:0]  magic sleep config word (see SLEEP_MODE_* constants)
            uint32_t rosc_pwd_deepsleep: 1;       // [16]    1 = power down ROSC in deep sleep
            uint32_t flash_pwd_sleep: 1;          // [17]    1 = power down flash in sleep
            uint32_t dco_pwd_sleep: 1;            // [18]    1 = power down DCO in sleep
            uint32_t reserved_19: 1;              // [19]
            uint32_t procore_dly: 4;              // [23:20] power-core-on delay before resuming CPU
            uint32_t reserved_24_31: 8;           // [31:24]
        };
    } sleep;

    union {
        uint32_t v;
        struct {
            uint32_t vdd_sleep: 3;                // [2:0]  digital VDD in sleep  (VDD_x_DOT_xx constants)
            uint32_t reserved_3: 1;               // [3]
            uint32_t vdd_active: 3;               // [6:4]  digital VDD when active
            uint32_t reserved_7_31: 25;           // [31:7]
        };
    } digital_vdd;

    union {
        uint32_t v;
        struct {
            uint32_t modem_pwd: 16;               // [15:0]  write MODEM_PWD to power down, MODEM_PWU to wake
            uint32_t mac_pwd: 16;                 // [31:16] write MAC_PWD to power down, MAC_PWU to wake
        };
    } power_mac_modem;

    union {
        uint32_t v;
        struct {
            uint32_t dsp_pwd: 16;                 // [15:0]  write DSP_PWD to power down, DSP_PWU to wake
            uint32_t reserved_16_31: 16;          // [31:16]
        };
    } power_dsp;

    union {
        uint32_t v;
        struct {
            uint32_t usb_pwd: 16;                 // [15:0]  write USB_PWD to power down, USB_PWU to wake
            uint32_t ble_pwd: 16;                 // [31:16] write BLE_PWD to power down, BLE_PWU to wake
        };
    } power_usb;

    union {
        uint32_t v;
        struct {
            uint32_t usb_power_down: 1;           // [0]     read-only; 1 = USB subsystem is powered down
            uint32_t dsp_power_down: 1;           // [1]     read-only; 1 = DSP subsystem is powered down
            uint32_t modem_power_down: 1;         // [2]     read-only; 1 = modem subsystem is powered down
            uint32_t mac_power_down: 1;           // [3]     read-only; 1 = MAC subsystem is powered down
            uint32_t reserved_4_31: 28;           // [31:4]
        };
    } pmu_status;

    union {
        uint32_t v;
        struct {
            uint32_t enable: 1;                   // [0]     1 = ROSC timer running
            uint32_t reserved_1_7: 7;             // [7:1]
            uint32_t int_status: 1;               // [8]     write 1 to clear interrupt
            uint32_t reserved_9_15: 7;            // [15:9]
            uint32_t period: 16;                  // [31:16] low 16 bits of timer period in ROSC cycles; high 16 in rosc_timer_h.period_h
        };
    } rosc_timer;

    uint32_t gpio_wakeup_en;                      // GPIO wake-up enable bitmask; bit N enables wake on GPIO N
    uint32_t gpio_wakeup_type;                    // wake-up edge: 0 = low/negedge, 1 = high/posedge per bit
    uint32_t gpio_wakeup_int_status;              // interrupt status; write 1s to clear

    union {
        uint32_t v;
        struct {
            uint32_t flash: 1;                    // [0]
            uint32_t dco: 1;                      // [1]
            uint32_t rosc_32k: 1;                 // [2]
            uint32_t xtal_26m: 1;                 // [3]
            uint32_t xtal_32k: 1;                 // [4]
            uint32_t dpll_480m: 1;                // [5]
            uint32_t digital_core: 1;             // [6]
            uint32_t digital_core_ldo_low_power: 1; // [7]
            uint32_t analog_sys_ldo: 1;           // [8]
            uint32_t io_ldo_low_power: 1;         // [9]
            uint32_t xtal_2_rf: 1;                // [10]
            uint32_t xtal_26m_low_power: 1;       // [11]
            uint32_t temprature_sensor: 1;        // [12]
            uint32_t saradc: 1;                   // [13]
            uint32_t usb: 1;                      // [14]   USB block enable
            uint32_t audio_random_generator: 1;   // [15]
            uint32_t audio_pll: 1;                // [16]
            uint32_t audio: 1;                    // [17]
            uint32_t mic_qspi_ram_or_flash: 1;    // [18]
            uint32_t nc: 1;                       // [19]
            uint32_t write_key: 12;               // [31:20] must write 0xA5C alongside any block enable change
        };
    } block_enable;

    union {
        uint32_t v;
        struct {
            uint32_t cal_trigger: 1;              // [0]     write 1 to trigger LDO bias calibration
            uint32_t reserved_1_3: 3;             // [3:1]
            uint32_t cal_manual: 1;               // [4]     1 = use manual calibration value (ldo_val_manual)
            uint32_t reserved_5_7: 3;             // [7:5]
            uint32_t ldo_val_manual: 5;           // [12:8]  manual LDO calibration value
            uint32_t reserved_13_15: 3;           // [15:13]
            uint32_t cal_out: 5;                  // [20:16] calibration result (read-only)
            uint32_t reserved_21_31: 11;          // [31:21]
        };
    } bias;

    union {
        uint32_t v;
        struct {
            uint32_t cal_en: 1;                   // [0]     ROSC calibration enable
            uint32_t cal_trig: 1;                 // [1]     write 1 to trigger one-shot calibration
            uint32_t cal_mode: 1;                 // [2]     0 = free-running, 1 = one-shot
            uint32_t reserved_3: 1;               // [3]
            uint32_t cal_interval: 3;             // [6:4]   auto-calibration interval selector
            uint32_t reserved_7_15: 9;            // [15:7]
            uint32_t manu_cal_en: 1;              // [16]    1 = use manu_cin / manu_fin values
            uint32_t manu_cin: 5;                 // [21:17] manual coarse calibration value
            uint32_t manu_fin: 9;                 // [30:22] manual fine calibration value
            uint32_t reserved_31: 1;              // [31]
        };
    } rosc_cal;

    union {
        uint32_t v;
        struct {
            uint32_t plug_in_en: 1;               // [0]     USB plug-in interrupt enable
            uint32_t plug_out_en: 1;              // [1]     USB plug-out interrupt enable
            uint32_t plug_in_int: 1;              // [2]     USB plug-in interrupt status; write 1 to clear
            uint32_t plug_out_int: 1;             // [3]     USB plug-out interrupt status; write 1 to clear
            uint32_t reserved_4_31: 28;           // [31:4]
        };
    } usb_plug_wakeup;

    uint32_t block_en_mux;                        // (NOT INVESTIGATED)

    union {
        uint32_t v;
        struct {
            uint32_t period_h: 16;                // [15:0]  high 16 bits of ROSC timer period; low 16 in rosc_timer.period
            uint32_t reserved_16_31: 16;          // [31:16]
        };
    } rosc_timer_h;

    uint32_t gpio_wakeup_en1;                     // GPIO wake-up enable for GPIO32..39
    uint32_t gpio_wakeup_type1;                   // wake-up edge for GPIO32..39
    uint32_t gpio_wakeup_int_status1;             // interrupt status for GPIO32..39; write 1s to clear

    union {
        uint32_t v;
        struct {
            uint32_t value: 16;                   // [15:0]  scratch; survives sleep and soft reset (jump to 0x0); cleared by WDT/power-on reset. On BK7221U the SDK uses 0x0080A080 (JPEG quant-table RAM) for start-type detection — that address survives WDT reset, this one does not
            uint32_t reserved_16_31: 16;          // [31:16] writes ignored, reads 0
        };
    } sw_retention;

} hw_sctrl_t;


static_assert(sizeof(hw_sctrl_t) == 85 * sizeof(uint32_t), "hw_sctrl_t size mismatch");

#define hw_sctrl                    ((volatile hw_sctrl_t *) SCTRL_BASE)


#define MODEM_SUBCHIP_RESET_WORD    (0x7111e802)
#define MAC_SUBSYS_RESET_WORD       (0x7111c802)
#define USB_SUBSYS_RESET_WORD       (0x7111c12B)
#define DSP_SUBSYS_RESET_WORD       (0x7111c410)
#define MODEM_CORE_RESET_WORD       (0xE802)

#define MAC_PWD                     (0xD802U)
#define MAC_PWU                     (0x0001U)
#define MODEM_PWD                   (0xD802U)
#define MODEM_PWU                   (0x0001U)
#define DSP_PWD                     (0xD410U)
#define DSP_PWU                     (0x0001U)
#define USB_PWD                     (0xD12BU)
#define USB_PWU                     (0x0001U)
#define BLE_PWD                     (0xDB1EU)
#define BLE_PWU                     (0x0001U)

#define LPO_SRC_ROSC                (0x0)
#define LPO_SRC_32K_XTAL            (0x1)
#define LPO_SRC_32K_DIV             (0x2)

#define SLEEP_MODE_NORMAL_VOL       (0x4F89)
#define SLEEP_MODE_LOW_VOL          (0xB706)
#define SLEEP_MODE_DEEP             (0xADC1)

#define VDD_1_DOT_51                (0x7)
#define VDD_1_DOT_38                (0x6)
#define VDD_1_DOT_25                (0x5)
#define VDD_1_DOT_12                (0x4)
#define VDD_0_DOT_99                (0x3)
#define VDD_0_DOT_86                (0x2)
#define VDD_0_DOT_73                (0x1)
#define VDD_0_DOT_60                (0x0)

#define SCTRL_BLOCK_ENABLE_WRITE_KEY    (0xA5C)


__unused static inline void sctrl_block_enable_temprature_sensor() {
    hw_write_fields(hw_sctrl->block_enable,
        .temprature_sensor = 1,
        .write_key = SCTRL_BLOCK_ENABLE_WRITE_KEY,
    );
}

__unused static inline void sctrl_block_disable_temprature_sensor() {
    hw_write_fields(hw_sctrl->block_enable,
        .temprature_sensor = 0,
        .write_key = SCTRL_BLOCK_ENABLE_WRITE_KEY,
    );
}
