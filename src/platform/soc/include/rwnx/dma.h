#pragma once

#include "platform/soc.h"


#define DMA_BASE_ADDR (0x10A00000)


typedef volatile struct {
    uint32_t ch_lli_root[4]; // 0x00..0x0C  per-channel LLI descriptor root pointer (RAM address of first LLI node)

    union {
        uint32_t v;
        struct {
            uint32_t oft_free         : 16; // [15:0]
            uint32_t ch0_stopped      : 1;  // [16]
            uint32_t ch1_stopped      : 1;  // [17]
            uint32_t ch2_stopped      : 1;  // [18]
            uint32_t ch3_stopped      : 1;  // [19]
            uint32_t request_state    : 4;  // [23:20]
            uint32_t arb_q0_valid     : 1;  // [24]
            uint32_t arb_q1_valid     : 1;  // [25]
            uint32_t arb_q2_valid     : 1;  // [26]
            uint32_t arb_q3_valid     : 1;  // [27]
            uint32_t up_stream_bsy    : 1;  // [28]
            uint32_t down_stream_bsy  : 1;  // [29]
            uint32_t reserved_30_31   : 2;  // [31:30]
        };
    } dma_status; // 0x10

    union {
        uint32_t v;
        struct {
            uint32_t lli_irq        : 16; // [15:0]
            uint32_t error          : 1;  // [16]
            uint32_t reserved_17_19 : 3;  // [19:17]
            uint32_t ch0_eot        : 1;  // [20]
            uint32_t ch1_eot        : 1;  // [21]
            uint32_t ch2_eot        : 1;  // [22]
            uint32_t ch3_eot        : 1;  // [23]
            uint32_t reserved_24_31 : 8;  // [31:24]
        };
    } int_rawstatus; // 0x14

    union {
        uint32_t v;
        struct {
            uint32_t lli_irq        : 16; // [15:0]
            uint32_t error          : 1;  // [16]
            uint32_t reserved_17_19 : 3;  // [19:17]
            uint32_t ch0_eot        : 1;  // [20]
            uint32_t ch1_eot        : 1;  // [21]
            uint32_t ch2_eot        : 1;  // [22]
            uint32_t ch3_eot        : 1;  // [23]
            uint32_t reserved_24_31 : 8;  // [31:24]
        };
    } int_unmask_set; // 0x18  write 1 to set interrupt mask bit

    union {
        uint32_t v;
        struct {
            uint32_t lli_irq        : 16; // [15:0]
            uint32_t error          : 1;  // [16]
            uint32_t reserved_17_19 : 3;  // [19:17]
            uint32_t ch0_eot        : 1;  // [20]
            uint32_t ch1_eot        : 1;  // [21]
            uint32_t ch2_eot        : 1;  // [22]
            uint32_t ch3_eot        : 1;  // [23]
            uint32_t reserved_24_31 : 8;  // [31:24]
        };
    } int_unmask_clear; // 0x1C  write 1 to clear interrupt mask bit

    union {
        uint32_t v;
        struct {
            uint32_t lli_irq        : 16; // [15:0]
            uint32_t error          : 1;  // [16]
            uint32_t reserved_17_19 : 3;  // [19:17]
            uint32_t ch0_eot        : 1;  // [20]
            uint32_t ch1_eot        : 1;  // [21]
            uint32_t ch2_eot        : 1;  // [22]
            uint32_t ch3_eot        : 1;  // [23]
            uint32_t reserved_24_31 : 8;  // [31:24]
        };
    } int_ack; // 0x20  write 1 to acknowledge (clear) interrupt

    union {
        uint32_t v;
        struct {
            uint32_t lli_irq        : 16; // [15:0]
            uint32_t error          : 1;  // [16]
            uint32_t reserved_17_19 : 3;  // [19:17]
            uint32_t ch0_eot        : 1;  // [20]
            uint32_t ch1_eot        : 1;  // [21]
            uint32_t ch2_eot        : 1;  // [22]
            uint32_t ch3_eot        : 1;  // [23]
            uint32_t reserved_24_31 : 8;  // [31:24]
        };
    } int_status; // 0x24  masked interrupt status (rawstatus AND unmask)

    uint32_t reserved_0x28_0x30[3]; // 0x28..0x30

    union {
        uint32_t v;
        struct {
            uint32_t ch0_priority      : 2;  // [1:0]
            uint32_t reserved_2_3      : 2;  // [3:2]
            uint32_t ch1_priority      : 2;  // [5:4]
            uint32_t reserved_6_7      : 2;  // [7:6]
            uint32_t ch2_priority      : 2;  // [9:8]
            uint32_t reserved_10_11    : 2;  // [11:10]
            uint32_t ch3_priority      : 2;  // [13:12]
            uint32_t reserved_14_15    : 2;  // [15:14]
            uint32_t interleave_enable : 1;  // [16]
            uint32_t reserved_17_31    : 15; // [31:17]
        };
    } channel_priority; // 0x34

    union {
        uint32_t v;
        struct {
            uint32_t ch0_mutex      : 1;  // [0]
            uint32_t ch1_mutex      : 1;  // [1]
            uint32_t ch2_mutex      : 1;  // [2]
            uint32_t ch3_mutex      : 1;  // [3]
            uint32_t reserved_4_31  : 28; // [31:4]
        };
    } channel_mutex_set; // 0x38  write 1 to acquire channel mutex

    union {
        uint32_t v;
        struct {
            uint32_t ch0_mutex      : 1;  // [0]
            uint32_t ch1_mutex      : 1;  // [1]
            uint32_t ch2_mutex      : 1;  // [2]
            uint32_t ch3_mutex      : 1;  // [3]
            uint32_t reserved_4_31  : 28; // [31:4]
        };
    } channel_mutex_clear; // 0x3C  write 1 to release channel mutex

    uint32_t reserved_0x40_0xbc[32]; // 0x40..0xBC  (NOT INVESTIGATED)

    union {
        uint32_t v;
        struct {
            uint32_t dummy          : 1;  // [0]
            uint32_t reserved_1_31  : 31; // [31:1]
        };
    } dummy; // 0xC0
} hw_dma_t;

#define hw_dma ((volatile hw_dma_t *)DMA_BASE_ADDR)
