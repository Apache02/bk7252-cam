#pragma once

#include <stdint.h>
#include <sys/cdefs.h>

#define _register_bits                     __attribute__((aligned(4))) __packed


#define FLASH_BASE                          (0x00803000)
#define REG_FLASH_OPERATE_SW                (FLASH_BASE + 0 * 4)
#define REG_FLASH_DATA_SW_FLASH             (FLASH_BASE + 1 * 4)
#define REG_FLASH_DATA_FLASH_SW             (FLASH_BASE + 2 * 4)
#define REG_FLASH_RDID_DATA_FLASH           (FLASH_BASE + 4 * 4)
#define REG_FLASH_SR_DATA_CRC_CNT           (FLASH_BASE + 5 * 4)
#define REG_FLASH_CONF                      (FLASH_BASE + 7 * 4)


typedef enum
{
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
} FLASH_OPCODE;


typedef struct {
    union operate_sw_u {
        uint32_t v;
        struct _register_bits {
            uint32_t addr: 24;                  // [0:23]
            uint32_t op_type_sw: 5;             // [24:28]
            uint32_t op_sw: 1;                  // [29]
            uint32_t wp: 1;                     // [30]
            uint32_t busy: 1;                   // [31]

        };
    } operate_sw;

    uint32_t data_sw_flash;
    uint32_t data_flash_sw;
    uint32_t reserved_3;
    uint32_t JEDEC_ID;

    union {
        uint32_t v;
        struct _register_bits {
            uint32_t sr: 8;                     // [0:7]
            uint32_t error_count: 8;            // [8:15]
            uint32_t data_flash_sw: 3;          // [16:18]
            uint32_t data_sw_flash: 3;          // [19:21]
            uint32_t reserved_22_31: 10;        // [22:31]
        };
    } sr_data_crc_cnt;

    uint32_t reserved_6;
    uint32_t conf;

} hw_flash_t;

#define hw_flash                ((volatile hw_flash_t *)FLASH_BASE)

