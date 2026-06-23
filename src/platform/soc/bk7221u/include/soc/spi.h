#pragma once

#include "platform/soc.h"


#define SPI_BASE_ADDR (0x00802700)


typedef volatile struct {
    union {
        uint32_t v;
        struct {
            uint32_t txint_mode: 2;     // [1:0]   TX FIFO interrupt trigger mode
            uint32_t rxint_mode: 2;     // [3:2]   RX FIFO interrupt trigger mode
            uint32_t txovr_en: 1;       // [4]     1 = TX overrun interrupt enabled
            uint32_t rxovr_en: 1;       // [5]     1 = RX overrun interrupt enabled
            uint32_t txint_en: 1;       // [6]     1 = TX FIFO interrupt enabled
            uint32_t rxint_en: 1;       // [7]     1 = RX FIFO interrupt enabled
            uint32_t ckr: 8;            // [15:8]  clock divider: f_spi = f_src / (2*(ckr+1))
            uint32_t nssmd: 2;          // [17:16] NSS (chip-select) mode
            uint32_t bit_wdth: 1;       // [18]    0 = 8-bit transfers, 1 = 16-bit transfers
            uint32_t reserved_19: 1;    // [19]
            uint32_t ckpol: 1;          // [20]    clock polarity (CPOL)
            uint32_t ckpha: 1;          // [21]    clock phase (CPHA)
            uint32_t msten: 1;          // [22]    1 = master mode, 0 = slave mode
            uint32_t spien: 1;          // [23]    1 = SPI enabled
            uint32_t reserved_24_31: 8; // [31:24]
        };
    } ctrl;

    union {
        uint32_t v;
        struct {
            uint32_t txfifo_empty: 1;    // [0]     TX FIFO empty
            uint32_t txfifo_full: 1;     // [1]     TX FIFO full
            uint32_t rxfifo_empty: 1;    // [2]     RX FIFO empty
            uint32_t rxfifo_full: 1;     // [3]     RX FIFO full
            uint32_t reserved_4_7: 4;    // [7:4]
            uint32_t txint: 1;           // [8]     TX FIFO interrupt pending; write 1 to clear
            uint32_t rxint: 1;           // [9]     RX FIFO interrupt pending; write 1 to clear
            uint32_t modf: 1;            // [10]    mode fault; write 1 to clear
            uint32_t txovr: 1;           // [11]    TX overrun; write 1 to clear
            uint32_t rxovr: 1;           // [12]    RX overrun; write 1 to clear
            uint32_t reserved_13: 1;     // [13]
            uint32_t slvsel: 1;          // [14]    slave-select pin state (read-only)
            uint32_t spibusy: 1;         // [15]    transfer in progress (read-only)
            uint32_t reserved_16_31: 16; // [31:16]
        };
    } stat;

    union {
        uint32_t v;
        struct {
            uint32_t word: 16;           // [15:0]  data word (8-bit or 16-bit per ctrl.bit_wdth)
            uint32_t reserved_16_31: 16; // [31:16]
        };
    } dat;

    union {
        uint32_t v;
        struct {
            uint32_t reserved_0: 1;       // [0]
            uint32_t cs_up_int_en: 1;     // [1]     1 = CS deassert interrupt enabled
            uint32_t reserved_2_3: 2;     // [3:2]
            uint32_t cs_up_int_status: 1; // [4]     CS deassert interrupt; write 1 to clear
            uint32_t reserved_5_31: 27;   // [31:5]
        };
    } slave_ctrl;
} hw_spi_t;

#define hw_spi ((volatile hw_spi_t *)SPI_BASE_ADDR)
