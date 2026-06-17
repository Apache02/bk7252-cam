#pragma once

#include "register_defs.h"
#include <stdint.h>

// ── SPI peripheral @ 0x00802700 ─────────────────────────────────────────────
// Register layout from SDK beken378/driver/spi/spi.h (SOC_BK7221U branch).
//
// When flash_spi_mux=1 (SCTRL_CONTROL bit[22]), the SPI peripheral drives
// GPIO14-17 directly (bypassing the GPIO peripheral).  GPIO14-17 must be
// set to high-impedance (GPIO_HIZ) first so their output drivers do not
// fight the SPI mux outputs.  CS# is controlled via NSSMD: 2=assert (NSS
// low), 3=deassert (NSS high).

typedef volatile struct {
    union {
        uint32_t v;
        struct {
            uint32_t txint_mode:     2; // [1:0]
            uint32_t rxint_mode:     2; // [3:2]
            uint32_t txovr_en:       1; // [4]
            uint32_t rxovr_en:       1; // [5]
            uint32_t txint_en:       1; // [6]
            uint32_t rxint_en:       1; // [7]
            uint32_t ckr:            8; // [15:8]  f = 26 MHz / (2*(ckr+1))
            uint32_t nssmd:          2; // [17:16] 2=NSS low (assert), 3=NSS high (deassert)
            uint32_t bit_wdth:       1; // [18]    0=8-bit, 1=16-bit
            uint32_t reserved_19:    1;
            uint32_t ckpol:          1; // [20]
            uint32_t ckpha:          1; // [21]
            uint32_t msten:          1; // [22]    1=master
            uint32_t spien:          1; // [23]    1=enable
            uint32_t reserved_24_31: 8;
        };
    } ctrl;

    union {
        uint32_t v;
        struct {
            uint32_t txfifo_empty:   1; // [0]
            uint32_t txfifo_full:    1; // [1]
            uint32_t rxfifo_empty:   1; // [2]
            uint32_t rxfifo_full:    1; // [3]
            uint32_t reserved_4_7:   4;
            uint32_t txint:          1; // [8]
            uint32_t rxint:          1; // [9]
            uint32_t modf:           1; // [10]
            uint32_t txovr:          1; // [11]
            uint32_t rxovr:          1; // [12]
            uint32_t reserved_13:    1;
            uint32_t slvsel:         1; // [14]
            uint32_t spibusy:        1; // [15]
            uint32_t reserved_16_31: 16;
        };
    } stat;

    uint32_t dat;          // [15:0] TX write / RX read; 8-bit when bit_wdth=0
    uint32_t slave_ctrl;
} hw_spi_bypass_t;

#define hw_spi_bp   ((hw_spi_bypass_t *)0x00802700u)

// ── GPIO individual pin config registers ─────────────────────────────────────
// GPIO_BASE = 0x00802800.  Each GPIO has a 32-bit config at base + gpio_num*4.
//
// Bit layout (SDK gpio.h GCFG_* defines):
//   [2] input_enable   — 1 = GPIO input buffer on
//   [3] output_enable  — 0 = output driver active, 1 = output tristated (SDK convention)
//   [4] pull_mode      — 0 = pull-down, 1 = pull-up
//   [5] pull_enable    — 1 = pull enabled
//   [6] function_enable — 1 = peripheral function selected
//
// SDK GMODE_SET_HIGH_IMPENDANCE writes 0x08 = bit[3]=1 only:
//   output tristated, input buffer off, no pull, no function → true Hi-Z.

#define GPIO_BASE_BP    0x00802800u
#define GPIO_REG(n)     (*(volatile uint32_t *)(GPIO_BASE_BP + (uint32_t)(n)*4u))
// SDK GMODE_SET_HIGH_IMPENDANCE = 0x08: output driver tristated, no input buffer.
#define GPIO_HIZ        0x08u

// ── ICU global interrupt enable @ ICU_BASE + 17*4 ───────────────────────────

#define GINTR_REG_BP        (*(volatile uint32_t *)(0x00802000u + 17u*4u))
