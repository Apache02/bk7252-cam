#pragma once

#include "register_defs.h"
#include <assert.h>

#define I2C1_BASE_ADDR             (0x0802300)
#define I2C2_BASE_ADDR             (0x0802600)


typedef volatile struct {
    union {
        uint32_t v;
        struct _register_bits {
            uint32_t ensmb: 1;                  // [0]
            uint32_t sta: 1;                    // [1]
            uint32_t sto: 1;                    // [2]
            uint32_t ack_tx: 1;                 // [3]
            uint32_t tx_mode: 1;                // [4]
            uint32_t reserved: 1;               // [5]
            uint32_t divider: 10;               // [6:15]
            uint32_t si: 1;                     // [16]
            uint32_t ack_rx: 1;                 // [17]
            uint32_t ack_req: 1;                // [18]
            uint32_t busy: 1;                   // [19]
            uint32_t reserved_20_31: 12;        // [20:31]
        };
    } config;

    union {
        uint32_t v;
        struct _register_bits {
            uint32_t byte: 8;                   // [0:7]
            uint32_t reserved_8_31: 24;         // [8:31]
        };
    } data;

} hw_i2c1_t;

#define hw_i2c1                    ((volatile hw_i2c1_t *) I2C1_BASE_ADDR)



static inline void i2c1_reset() {
    hw_i2c1->config.v = 0;
}

static inline void i2c1_set_divider(uint16_t div) {
    assert((div & ~(0x3FF)) == 0);
    hw_i2c1->config.divider = div & 0x3FF;
}
