#pragma once

#include "platform/soc.h"


#define LA_BASE_ADDR (0x10E00000)


typedef volatile struct {
    uint32_t id_low;                                 // 0x00

    uint32_t id_high;                                // 0x04

    uint32_t version;                                // 0x08

    union {
        uint32_t v;
        struct {
            uint32_t start        : 1;               // [0]
            uint32_t stop         : 1;               // [1]
            uint32_t reset        : 1;               // [2]
            uint32_t reserved_3_31 : 29;             // [31:3]
        };
    } cntrl;                                         // 0x0C

    union {
        uint32_t v;
        struct {
            uint32_t started      : 1;               // [0]
            uint32_t triggered    : 1;               // [1]
            uint32_t reserved_2_6 : 5;               // [6:2]
            uint32_t error        : 1;               // [7]
            uint32_t writeaddr    : 16;              // [23:8]
            uint32_t reserved_24_31 : 8;             // [31:24]
        };
    } status;                                        // 0x10

    uint32_t sampling_mask_low;                      // 0x14

    uint32_t sampling_mask_med;                      // 0x18

    uint32_t sampling_mask_high;                     // 0x1C

    uint32_t trigger_mask_low;                       // 0x20

    uint32_t trigger_mask_med;                       // 0x24

    uint32_t trigger_mask_high;                      // 0x28

    uint32_t trigger_value_low;                      // 0x2C

    uint32_t trigger_value_med;                      // 0x30

    uint32_t trigger_value_high;                     // 0x34

    union {
        uint32_t v;
        struct {
            uint32_t trigger_point  : 16;            // [15:0]
            uint32_t reserved_16_31 : 16;            // [31:16]
        };
    } trigger_point;                                 // 0x38

    union {
        uint32_t v;
        struct {
            uint32_t firstsample    : 16;            // [15:0]
            uint32_t reserved_16_31 : 16;            // [31:16]
        };
    } firstsample;                                   // 0x3C
} hw_la_t;

#define hw_la ((volatile hw_la_t *)LA_BASE_ADDR)
