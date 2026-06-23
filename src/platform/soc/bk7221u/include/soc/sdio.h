#pragma once

#include "platform/soc.h"


#define SDIO_BASE_ADDR (0x00801000)


typedef volatile struct {
    uint32_t reserved_0x00;

    union {
        uint32_t v;
        struct {
            uint32_t dma_en: 1;         // [0]     1 = DMA enabled
            uint32_t addr_ready: 1;     // [1]     1 = transfer address is ready
            uint32_t reserved_2_4: 3;   // [4:2]
            uint32_t intr_en_tx: 1;     // [5]     1 = TX data interrupt enabled
            uint32_t intr_en_rx: 1;     // [6]     1 = RX data interrupt enabled
            uint32_t intr_en_cmd: 1;    // [7]     1 = CMD interrupt enabled
            uint32_t reserved_8_31: 24; // [31:8]
        };
    } config;

    uint32_t addr; // [0x08] DMA transfer address

    union {
        uint32_t v;
        struct {
            uint32_t count: 16;          // [15:0]  transfer byte count
            uint32_t reserved_16_31: 16; // [31:16]
        };
    } count;

    uint32_t cmd_buf[16]; // [0x10–0x4C] command buffer (16 words)

    union {
        uint32_t v;
        struct {
            uint32_t tx_data: 1;        // [0]    target→host TX complete
            uint32_t rx_data: 1;        // [1]    host→target RX complete
            uint32_t cmd: 1;            // [2]    host→target CMD received
            uint32_t reserved_3_31: 29; // [31:3]
        };
    } intr_status;

    union {
        uint32_t v;
        struct {
            uint32_t rx_count: 20;      // [19:0]  received byte count
            uint32_t reserved_20_22: 3; // [22:20]
            uint32_t rx_valid: 1;       // [23]    received data is valid
            uint32_t blk_len: 3;        // [26:24] block length selector
            uint32_t reserved_27_31: 5; // [31:27]
        };
    } interactive_host;
} hw_sdio_t;

#define hw_sdio ((volatile hw_sdio_t *)SDIO_BASE_ADDR)
