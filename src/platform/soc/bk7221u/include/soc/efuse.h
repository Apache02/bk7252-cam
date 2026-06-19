#pragma once

#include "platform/soc.h"
#include "soc/sctrl.h"


#define EFUSE_BASE (SCTRL_BASE + 0x1D * 4)


typedef volatile struct {
    union {
        uint32_t v;
        struct {
            uint32_t en : 1;              // [0]      efuse operate enable, sw set 1, hw clear it after operate finished
            uint32_t dir : 1;             // [1]      =0: read, =1: write
            uint32_t reserved_2_7 : 6;    // [7:2]
            uint32_t addr : 5;            // [12:8]
            uint32_t reserved_13_15 : 3;  // [15:13]
            uint32_t wr_data : 5;         // [20:16]  efuse write data
            uint32_t reserved_21_31 : 11; // [31:21]
        };
    } ctrl;

    union {
        uint32_t v;
        struct {
            uint32_t rd_data : 8;        // [7:0]    efuse read data
            uint32_t is_valid : 1;       // [8]      read data valid indication
            uint32_t reserved_9_31 : 23; // [31:9]
        };
    } optr;

} hw_efuse_t;

#define hw_efuse ((volatile hw_efuse_t *)EFUSE_BASE)
