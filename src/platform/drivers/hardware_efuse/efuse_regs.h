#pragma once

#include "register_defs.h"
#include "hardware/sctrl_regs.h"


#define EFUSE_BASE         (SCTRL_BASE + 0x1D * 4)


typedef volatile struct {
    union {
        uint32_t v;
        struct _register_bits {
            uint32_t en: 1;                     // [0]      efuse operate enable, sw set 1, hw clear it after operate finished
            uint32_t dir: 1;                    // [1]      =0: read, =1: write
            uint32_t reserved_2_7: 6;           // [2:7]
            uint32_t addr: 5;                   // [8:12]
            uint32_t reserved_13_15: 3;         // [13:15]
            uint32_t wr_data: 5;                // [16:20]  efuse write data
            uint32_t reserved_21_31: 11;        // [21:31]
        };
    } ctrl;

    union {
        uint32_t v;
        struct _register_bits {
            uint32_t rd_data: 8;                // [0:7]    efuse read data
            uint32_t is_valid: 1;               // [8]      read data valid indication
            uint32_t reserved_9_31: 23;         // [9:31]
        };
    } optr;

} efuse_hw_t;


#define hw_efuse                ((volatile efuse_hw_t *)EFUSE_BASE)
