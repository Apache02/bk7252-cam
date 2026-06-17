// WORK IN PROGRESS — flash_uid() returns all-zero bytes.
// See docs/hardware/flash_bypass_investigation.md for root cause and proposed fix.
#include <stdint.h>
#include <stdbool.h>

#include "flash_bypass_regs.h"
#include "hardware/sctrl_regs.h"
#include "hardware/icu.h"
#include "hardware/flash_bypass.h"

// BK7252 MPU marks general RAM (0x00400000) non-executable; only IRAM (0x00900000)
// is executable.  This function must live in IRAM .text — i.e. it is only safe to
// call from IRAM firmware.  __noinline prevents the body from being copied into
// call-sites by the compiler.
//
// When flash_spi_mux=1 the SPI peripheral drives GPIO14-17 directly (bypassing the
// GPIO peripheral).  GPIO14-17 must be set to high-impedance beforehand so their
// output drivers do not fight the mux outputs — in particular, any active output
// driver on GPIO14 (MISO) would pull MISO to a fixed level, causing all reads to
// return 0x00.
__noinline
bool flash_bypass_xfer(const uint8_t *tx, uint8_t tx_len, uint8_t *rx, uint8_t rx_len) {
    // Copy tx to stack before the XIP gap.  tx may point into XIP flash which
    // becomes inaccessible when flash_spi_mux=1.  Volatile prevents a memcpy call.
    volatile uint8_t tx_buf[16];
    if (tx_len > sizeof(tx_buf)) return false;
    for (uint8_t i = 0; i < tx_len; i++) {
        tx_buf[i] = tx[i];
    }

    // 1. Disable global IRQ/FIQ — no interrupt handler may touch flash after step 5.
    uint32_t gintr_saved = GINTR_REG_BP;
    GINTR_REG_BP = 0;

    // 2. ICU: SPI clock from 26 MHz XTAL (safe before PLL), power up.
    hw_icu->peri_clk_mux.spi = PERI_CLK_26M_XTAL;
    hw_icu->peri_clk_pwd.spi = 0;

    // 3. SPI: master, Mode 0 (CKPOL=0 CKPHA=0), 8-bit, CKR=2 (~4.33 MHz),
    //    NSSMD=3 (NSS high = CS# deasserted), all interrupts disabled, enable.
    hw_spi_bp->ctrl.v = (2u << 8)     // CKR=2
                        | (3u << 16)  // NSSMD=3 (CS# deasserted)
                        | (1u << 22)  // MSTEN
                        | (1u << 23); // SPIEN

    // 4. Save GPIO14-17 and set to high-impedance (0x08 = output tristated, no input
    //    buffer).  This lets flash_spi_mux=1 drive/sample those pins exclusively.
    uint32_t gpio14_saved = GPIO_REG(14);
    uint32_t gpio15_saved = GPIO_REG(15);
    uint32_t gpio16_saved = GPIO_REG(16);
    uint32_t gpio17_saved = GPIO_REG(17);
    GPIO_REG(14) = GPIO_HIZ;
    GPIO_REG(15) = GPIO_HIZ;
    GPIO_REG(16) = GPIO_HIZ;
    GPIO_REG(17) = GPIO_HIZ;

    // 5. Switch flash SPI bus to SPI peripheral.  XIP unavailable from here.
    //    Loop until confirmed — write may be ignored if CPU is mid-fetch from flash.
    hw_sctrl->control.flash_spi_mux = 1;
    while (!hw_sctrl->control.flash_spi_mux)
        hw_sctrl->control.flash_spi_mux = 1;

    // 6. Assert CS# via NSS: NSSMD=2 drives NSS low.
    hw_spi_bp->ctrl.nssmd = 2;

    // 7. TX phase: clock out tx_buf, discard received bytes.
    for (uint8_t i = 0; i < tx_len; i++) {
        hw_spi_bp->dat = tx_buf[i];
        while (hw_spi_bp->stat.rxfifo_empty)
            ;
        (void)hw_spi_bp->dat;
    }

    // 8. RX phase: clock dummy 0x00 bytes, capture received bytes.
    for (uint8_t i = 0; i < rx_len; i++) {
        hw_spi_bp->dat = 0x00u;
        while (hw_spi_bp->stat.rxfifo_empty)
            ;
        rx[i] = (uint8_t)hw_spi_bp->dat;
    }

    // 9. Deassert CS# (NSSMD=3 drives NSS high) before restoring mux.
    hw_spi_bp->ctrl.nssmd = 3;

    // 10. Restore flash controller.  XIP available again.
    hw_sctrl->control.flash_spi_mux = 0;

    // 11. Restore GPIO14-17 to their saved state.
    GPIO_REG(14) = gpio14_saved;
    GPIO_REG(15) = gpio15_saved;
    GPIO_REG(16) = gpio16_saved;
    GPIO_REG(17) = gpio17_saved;

    // 12. Disable SPI, power down.
    hw_spi_bp->ctrl.v = 0;
    hw_icu->peri_clk_pwd.spi = 1;

    // 13. Restore IRQ/FIQ.
    GINTR_REG_BP = gintr_saved;

    return true;
}

bool flash_uid(uint8_t uid[16]) {
    // cmd is in IRAM .rodata — accessible during the XIP gap.
    static const uint8_t cmd[5] = {0x4Bu, 0x00u, 0x00u, 0x00u, 0x00u};
    return flash_bypass_xfer(cmd, sizeof(cmd), uid, 16);
}
