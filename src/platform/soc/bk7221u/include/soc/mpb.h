#pragma once

#include <stdint.h>
#include "platform/soc.h"


#define MPB_BASE  (0x01060000)
#define MPB_EXTRA (MPB_BASE + (4 * 128))

#define PPDU_RATE_POSI      (4)
#define PPDU_RATE_MASK      (0xF)
#define PPDU_BANDWIDTH_POSI (6)
#define PPDU_BANDWIDTH_MASK (0x1)


typedef volatile struct {
    union {
        uint32_t v;
        struct {
            uint32_t bypass_en: 1;      // [0]
            uint32_t reserved_1_3: 3;   // [3:1]
            uint32_t mode: 2;           // [5:4]
            uint32_t reserved_6_7: 2;   // [7:6]
            uint32_t single_txen: 1;    // [8]
            uint32_t reserved_9_31: 23; // [31:9]
        };
    } r0;

    union {
        uint32_t v;
        struct {
            uint32_t pre_rxdelay: 20;    // [19:0]
            uint32_t reserved_20_31: 12; // [31:20]
        };
    } r1;

    union {
        uint32_t v;
        struct {
            uint32_t rxdelay: 20;        // [19:0]
            uint32_t reserved_20_31: 12; // [31:20]
        };
    } r2;

    union {
        uint32_t v;
        struct {
            uint32_t pre_txdelay: 20;    // [19:0]
            uint32_t reserved_20_31: 12; // [31:20]
        };
    } r3;

    union {
        uint32_t v;
        struct {
            uint32_t txdelay: 20;        // [19:0]
            uint32_t reserved_20_31: 12; // [31:20]
        };
    } r4;

    uint32_t reserved_0x05_0x07[3];

    union {
        uint32_t v;
        struct {
            uint32_t rxstat: 20;         // [19:0]
            uint32_t reserved_20_31: 12; // [31:20]
        };
    } r8;

    union {
        uint32_t v;
        struct {
            uint32_t txstat: 20;         // [19:0]
            uint32_t reserved_20_31: 12; // [31:20]
        };
    } r9;

    union {
        uint32_t v;
        struct {
            uint32_t rxerrstat: 20;      // [19:0]
            uint32_t reserved_20_31: 12; // [31:20]
        };
    } r10;

    union {
        uint32_t v;
        struct {
            uint32_t txerrstat: 20;      // [19:0]
            uint32_t reserved_20_31: 12; // [31:20]
        };
    } r11;
} hw_mpb_t;

typedef volatile struct {
    union {
        uint32_t v;
        struct {
            uint32_t txvector0: 8;      // [7:0]
            uint32_t reserved_8_31: 24; // [31:8]
        };
    } r128;

    union {
        uint32_t v;
        struct {
            uint32_t txvector1: 8;      // [7:0]
            uint32_t reserved_8_31: 24; // [31:8]
        };
    } r129;

    union {
        uint32_t v;
        struct {
            uint32_t txvector2: 8;      // [7:0]
            uint32_t reserved_8_31: 24; // [31:8]
        };
    } r130;

    union {
        uint32_t v;
        struct {
            uint32_t txvector3: 8;      // [7:0]
            uint32_t reserved_8_31: 24; // [31:8]
        };
    } r131;

    union {
        uint32_t v;
        struct {
            uint32_t txvector4: 8;      // [7:0]
            uint32_t reserved_8_31: 24; // [31:8]
        };
    } r132;

    union {
        uint32_t v;
        struct {
            uint32_t txvector5: 8;      // [7:0]
            uint32_t reserved_8_31: 24; // [31:8]
        };
    } r133;

    union {
        uint32_t v;
        struct {
            uint32_t txvector6: 8;      // [7:0]
            uint32_t reserved_8_31: 24; // [31:8]
        };
    } r134;

    union {
        uint32_t v;
        struct {
            uint32_t txvector7: 8;      // [7:0]
            uint32_t reserved_8_31: 24; // [31:8]
        };
    } r135;

    union {
        uint32_t v;
        struct {
            uint32_t txvector8: 8;      // [7:0]
            uint32_t reserved_8_31: 24; // [31:8]
        };
    } r136;

    union {
        uint32_t v;
        struct {
            uint32_t txvector9: 8;      // [7:0]
            uint32_t reserved_8_31: 24; // [31:8]
        };
    } r137;

    union {
        uint32_t v;
        struct {
            uint32_t txvector10: 8;     // [7:0]
            uint32_t reserved_8_31: 24; // [31:8]
        };
    } r138;

    union {
        uint32_t v;
        struct {
            uint32_t txvector11: 8;     // [7:0]
            uint32_t reserved_8_31: 24; // [31:8]
        };
    } r139;

    union {
        uint32_t v;
        struct {
            uint32_t txvector12: 8;     // [7:0]
            uint32_t reserved_8_31: 24; // [31:8]
        };
    } r140;

    union {
        uint32_t v;
        struct {
            uint32_t txvector13: 8;     // [7:0]
            uint32_t reserved_8_31: 24; // [31:8]
        };
    } r141;

    union {
        uint32_t v;
        struct {
            uint32_t txvector14: 8;     // [7:0]
            uint32_t reserved_8_31: 24; // [31:8]
        };
    } r142;

    union {
        uint32_t v;
        struct {
            uint32_t txvector15: 8;     // [7:0]
            uint32_t reserved_8_31: 24; // [31:8]
        };
    } r143;

    uint32_t reserved_0x90_0x92[3];
} hw_mpb_extra_t;

#define hw_mpb       ((volatile hw_mpb_t *)MPB_BASE)
#define hw_mpb_extra ((volatile hw_mpb_extra_t *)MPB_EXTRA)
