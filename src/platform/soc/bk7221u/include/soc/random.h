#pragma once

#include "platform/soc.h"


#define TRNG_BASE_ADDR (0x00802480)


typedef volatile struct {
    union {
        uint32_t v;
        struct {
            uint32_t enable: 1;         // [0]     1 = TRNG running; keep set while reading
            uint32_t reserved_1_31: 31; // [31:1]
        };
    } ctrl;

    uint32_t data; // read-only; new random 32-bit word per read
} hw_trng_t;

#define hw_trng ((volatile hw_trng_t *)TRNG_BASE_ADDR)
