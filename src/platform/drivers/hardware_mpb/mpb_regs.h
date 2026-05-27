#pragma once

#include <stdint.h>
#include "register_defs.h"


#define MPB_BASE                            (0x01060000)
#define MPB_EXTRA                           (MPB_BASE + (4*128))

#define PPDU_RATE_POSI                      (4)
#define PPDU_RATE_MASK                      (0xF)
#define PPDU_BANDWIDTH_POSI                 (6)
#define PPDU_BANDWIDTH_MASK                 (0x1)


typedef volatile struct {
    union {
        struct _register_bits {
            uint32_t bypass_en: 1;
            uint32_t resv0: 3;
            uint32_t mode: 2;
            uint32_t resv1: 2;
            uint32_t single_txen: 1;
            uint32_t resv2: 23;
        };

        uint32_t v;
    } r0;

    union {
        struct _register_bits {
            uint32_t pre_rxdelay: 20;
            uint32_t resv2: 12;
        };

        uint32_t v;
    } r1;

    union {
        struct _register_bits {
            uint32_t rxdelay: 20;
            uint32_t resv2: 12;
        };

        uint32_t v;
    } r2;

    union {
        struct _register_bits {
            uint32_t pre_txdelay: 20;
            uint32_t resv2: 12;
        };

        uint32_t v;
    } r3;

    union {
        struct _register_bits {
            uint32_t txdelay: 20;
            uint32_t resv2: 12;
        };

        uint32_t v;
    } r4;

    uint32_t reserved_5;
    uint32_t reserved_6;
    uint32_t reserved_7;

    union {
        struct _register_bits {
            uint32_t rxstat: 20;
            uint32_t resv2: 12;
        };

        uint32_t v;
    } r8;

    union {
        struct _register_bits {
            uint32_t txstat: 20;
            uint32_t resv2: 12;
        };

        uint32_t v;
    } r9;

    union {
        struct _register_bits {
            uint32_t rxerrstat: 20;
            uint32_t resv2: 12;
        };

        uint32_t v;
    } r10;

    union {
        struct _register_bits {
            uint32_t txerrstat: 20;
            uint32_t resv2: 12;
        };

        uint32_t v;
    } r11;
} hw_mpb_reg_t;

typedef volatile struct {
    union {
        struct _register_bits {
            uint32_t txvector0: 8;
            uint32_t resv1: 24;
        };

        uint32_t v;
    } r128;

    union {
        struct _register_bits {
            uint32_t txvector1: 8;
            uint32_t resv1: 24;
        };

        uint32_t v;
    } r129;

    union {
        struct _register_bits {
            uint32_t txvector2: 8;
            uint32_t resv1: 24;
        };

        uint32_t v;
    } r130;

    union {
        struct _register_bits {
            uint32_t txvector3: 8;
            uint32_t resv1: 24;
        };

        uint32_t v;
    } r131;

    union {
        struct _register_bits {
            uint32_t txvector4: 8;
            uint32_t resv1: 24;
        };

        uint32_t v;
    } r132;

    union {
        struct _register_bits {
            uint32_t txvector5: 8;
            uint32_t resv1: 24;
        };

        uint32_t v;
    } r133;

    union {
        struct _register_bits {
            uint32_t txvector6: 8;
            uint32_t resv1: 24;
        };

        uint32_t v;
    } r134;

    union {
        struct _register_bits {
            uint32_t txvector7: 8;
            uint32_t resv1: 24;
        };

        uint32_t v;
    } r135;

    union {
        struct _register_bits {
            uint32_t txvector8: 8;
            uint32_t resv1: 24;
        };

        uint32_t v;
    } r136;

    union {
        struct _register_bits {
            uint32_t txvector9: 8;
            uint32_t resv1: 24;
        };

        uint32_t v;
    } r137;

    union {
        struct _register_bits {
            uint32_t txvector10: 8;
            uint32_t resv1: 24;
        };

        uint32_t v;
    } r138;

    union {
        struct _register_bits {
            uint32_t txvector11: 8;
            uint32_t resv1: 24;
        };

        uint32_t v;
    } r139;

    union {
        struct _register_bits {
            uint32_t txvector12: 8;
            uint32_t resv1: 24;
        };

        uint32_t v;
    } r140;

    union {
        struct _register_bits {
            uint32_t txvector13: 8;
            uint32_t resv1: 24;
        };

        uint32_t v;
    } r141;

    union {
        struct _register_bits {
            uint32_t txvector14: 8;
            uint32_t resv1: 24;
        };

        uint32_t v;
    } r142;

    union {
        struct _register_bits {
            uint32_t txvector15: 8;
            uint32_t resv1: 24;
        };

        uint32_t v;
    } r143;

    uint32_t reserved_144;
    uint32_t reserved_145;
    uint32_t reserved_146;
} hw_mpb_reg_extra_t;

#define hw_mpb              ((volatile hw_mpb_reg_t *) MPB_BASE)
#define hw_mpb_extra        ((volatile hw_mpb_reg_extra_t *) MPB_EXTRA)
