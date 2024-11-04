#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/cdefs.h>


#define SCTRL_BASE                  (0x00800000)

typedef struct {
    uint32_t chip_id;
    uint32_t device_id;

    union {
        uint32_t v;
        struct __attribute__((aligned(4))) __packed {
            uint32_t mclk_source: 2;            // [0:1]
            uint32_t hclk_div2: 2;              // [2:3]
            uint32_t divider: 4;                // [4:7]
            uint32_t flash_26m: 1;              // [8]
            uint32_t hclk_div2_en: 1;           // [9]
            uint32_t modem_clk480m_pwd: 1;      // [10]
            uint32_t mac_clk480m__pwd: 1;       // [11]
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

} hw_sctrl_t;

#define hw_sctrl                    ((volatile hw_sctrl_t *) SCTRL_BASE)
