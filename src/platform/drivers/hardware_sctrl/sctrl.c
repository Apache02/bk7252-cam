#include "soc/sctrl.h"
#include "hardware/sctrl.h"
#include "hardware/icu.h"


typedef enum {
    MCLK_SOURCE_DCO = 0,
    MCLK_SOURCE_26M_XTAL,
    MCLK_SOURCE_DPLL,
    MCLK_SOURCE_LPO,
} mclk_source_t;

#define DPLL_CLOCK_HZ       (480000000)
#define CPU_CLOCK_HZ        (120000000)
#define MCLK_DIVIDER        ((DPLL_CLOCK_HZ / CPU_CLOCK_HZ) - 1)


// ARM968E-S (5-stage pipeline, Thumb): `subs` = 1 cycle, taken `bcs` = 3 cycles,
// so one loop iteration is 4 cycles — subtract 4 per iteration to match.
static inline void busy_wait_at_least_cycles(unsigned long minimum_cycles) {
    __asm volatile(
        ".syntax unified\n"
        "1: subs %0, #4\n"
        "bcs 1b\n"
        : "+l" (minimum_cycles) : : "cc"
    );
}

// vendor's nested delay loop: one outer iteration is ~28 cycles
static inline void coarse_delay(uint32_t outer) {
    busy_wait_at_least_cycles(outer * 28);
}

// vendor: W32(0x0080012c, (R32(0x0080012c) & 0x000fffff) | 0xA5C00000 | bits)
// OR-style update that preserves already-enabled blocks, unlike
// hw_write_fields() which zero-fills every unmentioned field.
static void sctrl_block_enable_or(uint32_t bits) {
    typeof(hw_sctrl->block_enable) tmp;
    tmp.v = hw_sctrl->block_enable.v;
    tmp.write_key = SCTRL_BLOCK_ENABLE_WRITE_KEY;
    tmp.v |= bits;
    hw_sctrl->block_enable.v = tmp.v;
}


uint32_t chip_id() {
    return hw_sctrl->chip_id;
}

uint32_t device_id() {
    return hw_sctrl->device_id;
}

// Cold-boot clock bring-up, reverse-engineered from the vendor bootloader
// (FUN_000007c4 + FUN_000015e8). Fixed delays only — the DPLL has no lock
// bit to poll. Delay cycle counts assume the final 120 MHz clock; before
// the mclk switch the core runs slower, so the real time is longer — the
// values are minimums, that is harmless.
void sctrl_init() {
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

    hw_sctrl->power_mac_modem.v = 0;
    hw_sctrl->power_dsp.v       = 0;
    hw_sctrl->power_usb.v       = 0;
    coarse_delay(100);

    hw_write_fields(hw_sctrl->low_power_clk,
        .lpo_clk_mux = LPO_SRC_ROSC,
    );

    hw_write_fields(hw_sctrl->rosc_cal,
        .cal_en       = 1,
        .cal_trig     = 1,
        .cal_mode     = 1,
        .cal_interval = 3,
    );

    /* step 5: re-program analog with PLL enable bit (FUN_000015e8): | 0x08 */
    {
        typeof(hw_sctrl->block_enable) bits = {
            .xtal_26m = 1,
        };
        sctrl_block_enable_or(bits.v);
    }

    hw_sctrl->analog_ctrl0 = 0xF819A59B;
    hw_sctrl->analog_ctrl1 = 0x6AC03102;
    hw_sctrl->analog_ctrl2 = 0x24026040;
    hw_sctrl->analog_ctrl3 = 0x4FE06C50;
    hw_sctrl->analog_ctrl4 = 0x59C04520;

    busy_wait_at_least_cycles(1300);
    coarse_delay(100);

    // mclk_source + divider must change atomically — transient DPLL/1 = 480 MHz
    // if written separately, so preserve all other bits with read-modify-write.
    {
        typeof(hw_sctrl->control) reg;
        reg.v = hw_sctrl->control.v;
        reg.mclk_source = MCLK_SOURCE_DPLL;
        reg.divider = MCLK_DIVIDER;
        hw_sctrl->control.v = reg.v;
    }

    coarse_delay(100);

    // per-field RMW (unlike a whole-register write) keeps the other peripheral
    // clock muxes untouched
    icu_peri_clk_uart1(PERI_CLK_26M_XTAL);
    icu_peri_clk_uart2(PERI_CLK_26M_XTAL);
    icu_peri_clk_pwms(PERI_CLK_26M_XTAL);
    coarse_delay(100);

    hw_write_fields(hw_sctrl->bias,
        .cal_manual     = 1,
        .ldo_val_manual = 20,
    );
}


#define REG_RC_BASE_ADDR        (0x01050000)
#define RC_CNTL_STAT_ADDR       REG_RC_BASE_ADDR


void sctrl_rf_init() {
    hw_sctrl->modem_core_reset_phy_hclk.phy_hclk_enable = 1;
    hw_sctrl->control.modem_clk480m_pwd = 0;

    /*Enable BK7011:rc_en,ch0_en*/
    // rc_cntl_stat_set(0x09);
    (*(volatile uint32_t *) (RC_CNTL_STAT_ADDR)) = 0x09;
    // REG_PL_WR(RC_CNTL_STAT_ADDR, 0x09);
}
