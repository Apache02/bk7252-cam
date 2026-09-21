#include "soc/sctrl.h"
#include "soc/gpio.h"
#include "rwnx/rc.h"
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

// Delays here run either at DEFAULT_CPU_FREQ or, before the mclk switch, slower
// still — so counting cycles at the nominal rate always waits at least as long
// as intended.
#define CYCLES_PER_US (DEFAULT_CPU_FREQ / 1000000)

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

// The analog_ctrl* registers are not plain MMIO: each access is shifted out over
// an internal SPI link to the analog die, and analog_spi.state stays nonzero
// until that transfer retires. Writing again before it clears loses the write.
void sctrl_analog_set(volatile uint32_t *reg, uint32_t value) {
    *reg = value;
    while (hw_sctrl->analog_spi.state != 0);
}

uint32_t sctrl_analog_get(volatile const uint32_t *reg) {
    while (hw_sctrl->analog_spi.state != 0);
    return *reg;
}

// DPLL band calibration (vendor sctrl_cali_dpll(0)): drop and re-raise the SPI
// trigger, then the SPI detect enable, with a settling delay after each. The
// vendor's delay loops are named 10 us / 200 us but iterate a fixed count that
// undershoots at 120 MHz; these are honest minimums, and overshooting a
// settling delay is harmless.
#define ANALOG_CTRL0_SPI_TRIG (1u << 19)
#define ANALOG_CTRL0_SPI_DET_EN (1u << 4)

void sctrl_cali_dpll(void) {
    uint32_t param = sctrl_analog_get(&hw_sctrl->analog_ctrl0);

    param &= ~ANALOG_CTRL0_SPI_TRIG;
    sctrl_analog_set(&hw_sctrl->analog_ctrl0, param);
    busy_wait_at_least_cycles(10 * CYCLES_PER_US);

    param |= ANALOG_CTRL0_SPI_TRIG;
    sctrl_analog_set(&hw_sctrl->analog_ctrl0, param);

    param = sctrl_analog_get(&hw_sctrl->analog_ctrl0);
    param &= ~ANALOG_CTRL0_SPI_DET_EN;
    sctrl_analog_set(&hw_sctrl->analog_ctrl0, param);
    busy_wait_at_least_cycles(200 * CYCLES_PER_US);

    param = sctrl_analog_get(&hw_sctrl->analog_ctrl0);
    param |= ANALOG_CTRL0_SPI_DET_EN;
    sctrl_analog_set(&hw_sctrl->analog_ctrl0, param);
}

uint32_t chip_id() { return hw_sctrl->chip_id; }

uint32_t device_id() { return hw_sctrl->device_id; }

// Application-side system-control bring-up, ported from the vendor SDK's
// sctrl_init() (the reference build, driver/sys_ctrl/sys_ctrl.c) — the firmware
// this board is known to run WiFi on. Cold boot is not repeated here: the
// bootloader already brought the clocks up in bootloader_sctrl_init(), which is
// why the two are separate functions and free to diverge.
//
// Two deliberate departures from that vendor routine, both outside the RF path:
//   - it runs the core off the DCO at 180 MHz (USE_DCO_CLK_POWON is 1 for
//     BK7221U); we stay on the DPLL at DEFAULT_CPU_FREQ, so its sctrl_dco_cali()
//     and sctrl_set_cpu_clk_dco() have nothing to calibrate here.
//   - MAC/modem power-up, the MAC clock gates and the modem resets live in
//     sctrl_rf_init() instead, so apps that never touch WiFi keep those blocks
//     powered down.
void sctrl_init() {
    // Keep the 26M XTAL block on — it also protects the 32k circuit.
    {
        typeof(hw_sctrl->block_enable) bits = {
            .dco       = 1,
            .xtal_26m  = 1,
            .dpll_480m = 1,
            .xtal_2_rf = 1,
        };
        sctrl_block_enable_or(bits.v);
    }

    hw_write_fields(hw_sctrl->low_power_clk,
        .lpo_clk_mux = LPO_SRC_ROSC,
    );

    hw_write_fields(hw_sctrl->rosc_cal,
        .cal_en = 1,
        .cal_trig = 1,
        .cal_mode = 1,
        .cal_interval = 3,
    );

    // mclk_source + divider must change atomically — transient DPLL/1 = 480 MHz
    // if written separately, so preserve all other bits with read-modify-write.
    sctrl_set_cpu_freq_hz(DEFAULT_CPU_FREQ);
    coarse_delay(100);

    // LDO bias calibration word, verbatim from the vendor (SCTRL_BIAS =
    // 0x00171710): manual mode with LDO value 23. Written as a whole word
    // because it also sets the field our register model marks read-only.
    hw_sctrl->bias.v = 0x00171710;

    // analog_ctrl0 differs from the value the bootloader leaves behind
    // (0xF819A59B, taken from the vendor bootloader): the vendor application
    // overwrites bits [31:28] on the way up, so 0x0819A59B is what the
    // known-good firmware actually runs the radio on.
    sctrl_analog_set(&hw_sctrl->analog_ctrl0, 0x0819A59B);
    sctrl_cali_dpll();
    sctrl_analog_set(&hw_sctrl->analog_ctrl1, 0x6AC03102);
    // 0x24026080 with XTALH_CTUNE (bits [7:2]) forced to the vendor's 0x10;
    // flash calibration overwrites it later via CMD_SCTRL_SET_XTALH_CTUNE.
    sctrl_analog_set(&hw_sctrl->analog_ctrl2, 0x24026040);
    sctrl_analog_set(&hw_sctrl->analog_ctrl3, 0x4FE06C50);
    sctrl_analog_set(&hw_sctrl->analog_ctrl4, 0x59C04520);

    busy_wait_at_least_cycles(1300);
    coarse_delay(100);

    // per-field RMW (unlike a whole-register write) keeps the other peripheral
    // clock muxes untouched
    icu_uart1_clk(PERI_CLK_26M_XTAL);
    icu_uart2_clk(PERI_CLK_26M_XTAL);
    icu_pwms_clk(PERI_CLK_26M_XTAL);
    coarse_delay(100);

    // Raise digital VDD to the vendor's active-mode level (CMD_SCTRL_SET_VDD_VALUE
    // with 5). Field assignment, not hw_write_fields(), so vdd_sleep survives —
    // the vendor read-modify-writes this register too.
    hw_sctrl->digital_vdd.vdd_active = 5;

    // Marks that this boot ran past init without a real WDT/POR reset in between,
    // since only those clear this register - lets boot diagnostics tell a live
    // jump into a fresh image apart from an actual reset.
    hw_sctrl->sw_retention.value = 0xA5A5;
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
    intc_register_irq_handler(FIQ_SOURCE_DPLL_UNLOCK, sctrl_dpll_isr);
    intc_enable_irq_source(FIQ_SOURCE_DPLL_UNLOCK);
    gpio_extra_int_set(dpll_unlock_int_en = 1);
}

void sctrl_dpll_int_close(void) {
    intc_disable_irq_source(FIQ_SOURCE_DPLL_UNLOCK);
    intc_unregister_irq_handler(FIQ_SOURCE_DPLL_UNLOCK, sctrl_dpll_isr);
    gpio_extra_int_set(dpll_unlock_int_en = 0);
    gpio_extra_int_clear(dpll_unlock_int);
}

void sctrl_subsys_modem_reset() {
    hw_sctrl->modem_subchip_reset_request = MODEM_SUBCHIP_RESET_WORD;
    coarse_delay(10);
    hw_sctrl->modem_subchip_reset_request = 0;
}

void sctrl_subsys_mac_reset() {
    hw_sctrl->mac_subsys_reset_request = MAC_SUBSYS_RESET_WORD;
    coarse_delay(10);
    hw_sctrl->mac_subsys_reset_request = 0;
}

void sctrl_subsys_usb_reset() {
    hw_sctrl->usb_subsys_reset_request = USB_SUBSYS_RESET_WORD;
    coarse_delay(10);
    hw_sctrl->usb_subsys_reset_request = 0;
}

void sctrl_subsys_dsp_reset() {
    hw_sctrl->dsp_subsys_reset_request = DSP_SUBSYS_RESET_WORD;
    coarse_delay(10);
    hw_sctrl->dsp_subsys_reset_request = 0;
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

// Brings the WiFi side of the chip out of the powered-down state the bootloader
// leaves it in. Split out of sctrl_init() so apps that never use WiFi keep MAC
// and modem dark. Ported from the vendor's sctrl_init() tail plus
// sctrl_rf_wakeup() (the reference build, driver/sys_ctrl/sys_ctrl.c).
void sctrl_rf_init() {
    // Power domains: the *_PWU key wakes a block, and the two halves of this
    // register must be written together or the untouched half reads back as a
    // key of 0, which is neither PWD nor PWU.
    hw_write_fields(hw_sctrl->power_mac_modem,
        .mac_pwd = MAC_PWU,
        .modem_pwd = MODEM_PWU,
    );
    coarse_delay(100);

    // vendor sctrl_sub_reset(): the modem-side resets, minus the USB/DSP ones
    // that have nothing to do with the radio.
    hw_sctrl->control.mpif_clk_invert = 1;
    sctrl_modem_core_reset();
    sctrl_subsys_modem_reset();
    sctrl_subsys_mac_reset();

    // "sys ctrl clk gating, for rx dma dead" — vendor writes 0x3F, ungating the
    // six MAC clock domains that feed the RX path.
    hw_write_fields(hw_sctrl->clk_gating,
        .mac_mpif = 1,
        .mac_wt = 1,
        .mac_core_rx = 1,
        .mac_core_tx = 1,
        .mac_crypt = 1,
        .mac_pri = 1,
    );

    // MAC then modem, each an AHB slave clock followed by its 480 MHz subsystem
    // clock — the order the vendor wakes them in.
    hw_sctrl->modem_core_reset_phy_hclk.mac_hclk_enable = 1;
    hw_sctrl->control.mac_clk480m_pwd                   = 0;
    hw_sctrl->modem_core_reset_phy_hclk.phy_hclk_enable = 1;
    hw_sctrl->control.modem_clk480m_pwd                 = 0;

    // Enable the BK7011 radio controller (vendor rc_cntl_stat_set(0x09)).
    hw_write_fields(hw_rc->cntl_stat,
        .ch0_en = 1,
        .rc_en = 1,
    );
}
