#include <stdint.h>
#include "hardware/sctrl.h"
#include "hardware/icu.h"
#include "soc/sctrl.h"

// ARM968E-S (5-stage pipeline, Thumb): `subs` = 1 cycle, taken `bcs` = 3 cycles,
// so one loop iteration is 4 cycles — subtract 4 per iteration to match.
static inline void busy_wait_at_least_cycles(unsigned long minimum_cycles) {
    __asm volatile(".syntax unified\n"
                   "1: subs %0, #4\n"
                   "bcs 1b\n"
                   : "+l"(minimum_cycles)
                   :
                   : "cc");
}

// vendor's nested delay loop: one outer iteration is ~28 cycles
static inline void coarse_delay(uint32_t outer) { busy_wait_at_least_cycles(outer * 28); }

// vendor: W32(0x0080012c, (R32(0x0080012c) & 0x000fffff) | 0xA5C00000 | bits)
// OR-style update that preserves already-enabled blocks, unlike
// hw_write_fields() which zero-fills every unmentioned field.
static void sctrl_block_enable_or(uint32_t bits) {
    typeof(hw_sctrl->block_enable) tmp;
    tmp.v         = hw_sctrl->block_enable.v;
    tmp.write_key = SCTRL_BLOCK_ENABLE_WRITE_KEY;
    tmp.v |= bits;
    hw_sctrl->block_enable.v = tmp.v;
}

static inline void set_analog_spi(volatile uint32_t *reg, uint32_t value) {
    *reg = value;
    while (hw_sctrl->analog_spi.state != 0);
}

// Cold-boot clock bring-up, reverse-engineered from the vendor bootloader
// (FUN_000007c4 + FUN_000015e8). Fixed delays only — the DPLL has no lock
// bit to poll. Delay cycle counts assume the final 120 MHz clock; before
// the mclk switch the core runs slower, so the real time is longer — the
// values are minimums, that is harmless.
void bootloader_sctrl_init() {
    /* steps 1-2: analog bandgap/bias preset (FUN_000007c4): | 0x43F */
    {
        typeof(hw_sctrl->block_enable) bits = {
            .flash     = 1,
            .dco       = 1,
            .rosc_32k  = 1,
            .xtal_26m  = 1,
            .xtal_32k  = 1,
            .dpll_480m = 1,
            .xtal_2_rf = 1,
        };
        sctrl_block_enable_or(bits.v);
    }

    // resets
    hw_sctrl->modem_subchip_reset_request = MODEM_SUBCHIP_RESET_WORD;
    hw_sctrl->mac_subsys_reset_request = MAC_SUBSYS_RESET_WORD;
    hw_sctrl->usb_subsys_reset_request = USB_SUBSYS_RESET_WORD;
    hw_sctrl->dsp_subsys_reset_request = DSP_SUBSYS_RESET_WORD;
    coarse_delay(10);
    hw_sctrl->modem_subchip_reset_request = 0;
    hw_sctrl->mac_subsys_reset_request = 0;
    hw_sctrl->usb_subsys_reset_request = 0;
    hw_sctrl->dsp_subsys_reset_request = 0;

    // power down
    hw_write_fields(hw_sctrl->power_mac_modem,
        .mac_pwd = MAC_PWD,
        .modem_pwd = MODEM_PWD,
    );
    hw_write_fields(hw_sctrl->power_dsp,
        .dsp_pwd = DSP_PWD,
    );
    hw_write_fields(hw_sctrl->power_usb,
        .ble_pwd = BLE_PWD,
        .usb_pwd = USB_PWD,
    );

    coarse_delay(100);

    // Low power clk
    hw_write_fields(hw_sctrl->low_power_clk,
        .lpo_clk_mux = LPO_SRC_ROSC,
    );

    hw_write_fields(hw_sctrl->rosc_cal,
        .cal_en = 1,
        .cal_trig = 1,
        .cal_mode = 1,
        .cal_interval = 3,
    );

    /* step 5: re-program analog with PLL enable bit (FUN_000015e8): | 0x08 */
    {
        typeof(hw_sctrl->block_enable) bits = {
            .xtal_26m = 1,
        };
        sctrl_block_enable_or(bits.v);
    }

    set_analog_spi(&hw_sctrl->analog_ctrl0, 0xF819A59B);
    set_analog_spi(&hw_sctrl->analog_ctrl1, 0x6AC03102);
    set_analog_spi(&hw_sctrl->analog_ctrl2, 0x24026040);
    set_analog_spi(&hw_sctrl->analog_ctrl3, 0x4FE06C50);
    set_analog_spi(&hw_sctrl->analog_ctrl4, 0x59C04520);

    busy_wait_at_least_cycles(1300);
    coarse_delay(100);

    // mclk_source + divider must change atomically — transient DPLL/1 = 480 MHz
    // if written separately, so preserve all other bits with read-modify-write.
    sctrl_set_cpu_freq_hz(DEFAULT_CPU_FREQ);

    coarse_delay(100);

    // per-field RMW (unlike a whole-register write) keeps the other peripheral
    // clock muxes untouched
    icu_uart1_clk(PERI_CLK_26M_XTAL);
    icu_uart2_clk(PERI_CLK_26M_XTAL);
    icu_pwms_clk(PERI_CLK_26M_XTAL);
    coarse_delay(100);

    hw_write_fields(hw_sctrl->bias,
        .cal_manual = 1,
        .ldo_val_manual = 20,
    );

    // Marks that this boot ran past init without a real WDT/POR reset in between,
    // since only those clear this register - lets boot diagnostics tell a live
    // jump into a fresh image apart from an actual reset.
    hw_sctrl->sw_retention.value = 0xA5A5;
}
