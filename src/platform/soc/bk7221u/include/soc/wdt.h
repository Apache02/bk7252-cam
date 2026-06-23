#pragma once

#include "platform/soc.h"


#define WDT_BASE_ADDR (0x00802900)

#define WDT_KEY_1ST (0x5A)
#define WDT_KEY_2ND (0xA5)


typedef volatile struct {
    union {
        uint32_t v;
        struct {
            uint32_t period: 16;        // [15:0]  timeout period in cycles
            uint32_t key: 8;            // [23:16] write WDT_KEY_1ST then WDT_KEY_2ND to reload
            uint32_t reserved_24_31: 8; // [31:24]
        };
    } ctrl;
} hw_wdt_t;

#define hw_wdt ((volatile hw_wdt_t *)WDT_BASE_ADDR)
