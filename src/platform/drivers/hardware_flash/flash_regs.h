#pragma once

#include "register_defs.h"

#define FLASH_BASE                          (0x00803000)


typedef enum {
    FLASH_OPCODE_WREN    = 1,
    FLASH_OPCODE_WRDI    = 2,
    FLASH_OPCODE_RDSR    = 3,
    FLASH_OPCODE_WRSR    = 4,
    FLASH_OPCODE_READ    = 5,
    FLASH_OPCODE_RDSR2   = 6,
    FLASH_OPCODE_WRSR2   = 7,
    FLASH_OPCODE_PP      = 12,
    FLASH_OPCODE_SE      = 13,
    FLASH_OPCODE_BE1     = 14,
    FLASH_OPCODE_BE2     = 15,
    FLASH_OPCODE_CE      = 16,
    FLASH_OPCODE_DP      = 17,
    FLASH_OPCODE_RFDP    = 18,
    FLASH_OPCODE_RDID    = 20,
    FLASH_OPCODE_HPM     = 21,
    FLASH_OPCODE_CRMR    = 22,
    FLASH_OPCODE_CRMR2   = 23,
    FLASH_OPCODE_RDSCR   = 28,
} FLASH_OPCODE;


typedef volatile struct {

    /* offset 0 */
    union {
        uint32_t v;
        struct {
            uint32_t       addr: 24;        // [23:0]   24-bit byte address passed to flash chip
            uint32_t       op_type_sw: 5;   // [28:24]  opcode identifier; see FLASH_OPCODE
            uint32_t       op_sw: 1;        // [29]     write 1 to issue command; self-clearing
            uint32_t       wp: 1;           // [30]     1 = de-assert WP# pin; required for PP/SE
            const uint32_t busy: 1;         // [31]     1 while operation in progress
        };
    } operate_sw;

    uint32_t       data_sw_flash;           // [offset 1]  write FIFO: SW→flash, 8 words (one 32-byte block per PP)
    const uint32_t data_flash_sw;           // [offset 2]  read FIFO: flash→SW, 8 words (one 32-byte block per READ)
    uint32_t       reserved_0x03;

    const uint32_t JEDEC_ID;               // [offset 4]  JEDEC ID captured by last RDID command

    /* offset 5 */
    union {
        uint32_t v;
        struct {
            const uint32_t sr: 8;           // [7:0]    SR byte captured by last RDSR or RDSR2
            const uint32_t error_count: 8;  // [15:8]   (NOT INVESTIGATED) never observed to increment (see flash.md)
            const uint32_t rd_fifo_ptr: 3;  // [18:16]  read FIFO word pointer: auto-increments on each offset-2 read; reset to 0 by READ command
            const uint32_t wr_fifo_ptr: 3;  // [21:19]  write FIFO status; reads 7 constantly regardless of writes to offset-1; writes ignored (confirmed on hardware)
            uint32_t       m_value: 8;      // [29:22]  M-byte for quad continuous-read; set per flash chip datasheet
            uint32_t       page_write_en: 1;// [30]     (NOT INVESTIGATED) sticky on BK7221U; SDK uses only on BK7238/BK7252N
            uint32_t       reserved_31: 1;  // [31]
        };
    } sr_data_crc_cnt;

    uint32_t reserved_0x06;

    /* offset 7 */
    union {
        uint32_t v;
        struct {
            uint32_t clk_conf: 4;           // [3:0]    SPI clock selection; 5 = 60 MHz (DPLL source)
            uint32_t model_sel: 5;          // [8:4]    XIP read mode: 0 = single, 1 = dual, 2 = quad
            uint32_t cpu_data_wr: 1;        // [9]      (NOT INVESTIGATED) SDK name: FWREN_FLASH_CPU
            uint32_t wrsr_data: 16;         // [25:10]  SR payload: low byte → WRSR; both bytes → WRSR2
            uint32_t crc_en: 1;             // [26]     0 = disable XIP CRC validation (no abort, addr mapping unchanged); 1 = abort on mismatch
            uint32_t reserved_27_31: 5;     // [31:27]
        };
    } conf;

    uint32_t reserved_0x08;                 // BK7252N deep-power-down config (REG_FLASH_DPD_CONF); reserved on BK7221U
    uint32_t reserved_0x09;                 // BK7252N page-write buffer config (REG_FLASH_PW_CONF); reserved on BK7221U — writes ignored (confirmed on hardware)

} hw_flash_t;

#define hw_flash                ((volatile hw_flash_t *) FLASH_BASE)
