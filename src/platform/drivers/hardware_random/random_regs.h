#pragma once

#include "register_defs.h"


#define TRNG_BASE_ADDR        (0x00802480)


typedef volatile struct {
    union {
        uint32_t v;
        struct _register_bits {
            uint32_t enable: 1;
            uint32_t reserved_1_31: 31;
        };
    } ctrl;

    uint32_t data;
} hw_trng_t;

#define hw_trng                ((volatile hw_trng_t *)TRNG_BASE_ADDR)
