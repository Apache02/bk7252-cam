#include "soc/sctrl.h"
#include "soc/gpio.h"
#include "hardware/sctrl.h"
#include "hardware/icu.h"
#include "hardware/intc.h"


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

uint32_t chip_id() { return hw_sctrl->chip_id; }

uint32_t device_id() { return hw_sctrl->device_id; }

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

    hw_sctrl->analog_ctrl0 = 0xF819A59B;
    hw_sctrl->analog_ctrl1 = 0x6AC03102;
    hw_sctrl->analog_ctrl2 = 0x24026040;
    hw_sctrl->analog_ctrl3 = 0x4FE06C50;
    hw_sctrl->analog_ctrl4 = 0x59C04520;

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
}

// The DPLL-unlock latch lives in GPIO extra_int_cfg, not in the ICU: clearing
// the ICU status alone leaves it asserted and the FIQ re-fires forever. This
// handler exists to acknowledge it.
//
// The SDK (sys_ctrl.c::sctrl_dpll_isr) additionally falls the flash clock and
// mclk back from DPLL to DCO so the core survives a real loss of lock. That
// fallback is not ported — an unlock event here is logged only by the fact that
// the interrupt fired, and the clocks stay on the DPLL.
static void sctrl_dpll_isr(void) { gpio_extra_int_clear(dpll_unlock_int); }

void sctrl_dpll_int_open(void) {
    gpio_extra_int_clear(dpll_unlock_int);
    intc_register_fiq_handler(FIQ_SOURCE_DPLL_UNLOCK, sctrl_dpll_isr);
    intc_enable_fiq_source(FIQ_SOURCE_DPLL_UNLOCK);
    gpio_extra_int_set(dpll_unlock_int_en = 1);
}

void sctrl_dpll_int_close(void) {
    intc_disable_fiq_source(FIQ_SOURCE_DPLL_UNLOCK);
    intc_unregister_fiq_handler(FIQ_SOURCE_DPLL_UNLOCK, sctrl_dpll_isr);
    gpio_extra_int_set(dpll_unlock_int_en = 0);
    gpio_extra_int_clear(dpll_unlock_int);
}

// Pulses the modem-core reset. Ported from SDK sys_ctrl.c, case
// CMD_SCTRL_MODEM_CORE_RESET: write MODEM_CORE_RESET_WORD into reset_word
// (preserving phy_hclk_enable/mac_hclk_enable), wait, clear reset_word back
// to 0, then poll reset.modem_core_reset until hardware deasserts it.
void sctrl_modem_core_reset(void) {
    typeof(hw_sctrl->modem_core_reset_phy_hclk) reg;
    reg.v = hw_sctrl->modem_core_reset_phy_hclk.v;

    reg.reset_word                        = MODEM_CORE_RESET_WORD;
    hw_sctrl->modem_core_reset_phy_hclk.v = reg.v;

    // SDK's delay(1) is a fixed instruction-count busy loop, not a calibrated
    // time — it never checks frequency either. 0x400 cycles approximates its
    // own loop body (100 volatile-counter iterations x ~7 instructions each).
    busy_wait_at_least_cycles(0x400); // approx SDK delay(1)

    reg.reset_word                        = 0;
    hw_sctrl->modem_core_reset_phy_hclk.v = reg.v;

    while (hw_sctrl->reset.modem_core_reset) {
        busy_wait_at_least_cycles(0x400 * 8); // approx SDK delay(10)
    }
}

void sctrl_overclock(__unused bool enable) {
    // SDK (BK7221U branch): refcounted toggle around MCU power-save clock
    // division, not a real "overclock". enable=1 calls sctrl_mcu_exit() to
    // force the full DPLL clock while mcu_ps_is_on(); enable=0 re-arms
    // sctrl_mcu_init() to drop back to the power-save divided clock once the
    // last caller releases it (refcounted, so nested enable/disable pairs
    // nest correctly).
    //
    // This project has no power-save mode yet (see port_wifi/power_save.c:
    // "stage 1 — RF always on", all predicates/setters are stubs), so the
    // CPU is assumed to already run at its nominal frequency at all times.
    // Nothing to do here until MCU power-save (mcu_ps_is_on/sctrl_mcu_init/
    // sctrl_mcu_exit, gated by CFG_USE_MCU_PS in the SDK) is implemented.
}

#define REG_RC_BASE_ADDR  (0x01050000)
#define RC_CNTL_STAT_ADDR REG_RC_BASE_ADDR

void sctrl_rf_init() {
    hw_sctrl->modem_core_reset_phy_hclk.phy_hclk_enable = 1;
    hw_sctrl->control.modem_clk480m_pwd                 = 0;

    /*Enable BK7011:rc_en,ch0_en*/
    // rc_cntl_stat_set(0x09);
    (*(volatile uint32_t *)(RC_CNTL_STAT_ADDR)) = 0x09;
    // REG_PL_WR(RC_CNTL_STAT_ADDR, 0x09);
}
