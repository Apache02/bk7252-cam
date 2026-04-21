#pragma once

#include <stdint.h>
#include "register_defs.h"


#define SECURITY_BASE                   (0x00806000)
#define SECURITY_AES_BASE               (SECURITY_BASE + 0 * 4)
#define SECURITY_SHA_BASE               (SECURITY_BASE + 0x40 * 4)
#define SECURITY_RSA_BASE               (SECURITY_BASE + 0x80 * 4)


typedef volatile struct {
    uint32_t key_0to7[8];

    union {
        struct _register_bits {
            uint32_t init: 1;
            uint32_t next: 1;
            uint32_t auteo: 1;
            uint32_t int_en: 1;
            uint32_t reserved_4_31: 28;
        };

        uint32_t v;
    } control;

    union {
        struct _register_bits {
            uint32_t ready: 1;
            uint32_t valid: 1;
            uint32_t int_flag: 1;
            uint32_t reserved_3_31: 29;
        };

        uint32_t v;
    } status;

    union {
        struct _register_bits {
            uint32_t encode: 1;
            uint32_t mode: 2;
            uint32_t reserved_3_31: 29;
        };

        uint32_t v;
    } config;

    uint32_t reserved_0xB;

    uint32_t block_0to3[4];
    uint32_t result_0to3[4];
} hw_security_aes_reg_t;

typedef volatile struct {
    uint32_t block_31to0[32];
    uint32_t digest_15to0[16];

    union {
        struct _register_bits {
            uint32_t init: 1;
            uint32_t next: 1;
            uint32_t reserved_2_31: 30;
        };

        uint32_t v;
    } control;

    union {
        struct _register_bits {
            uint32_t ready: 1;
            uint32_t valid: 1;
            uint32_t int_flag: 1;
            uint32_t reserved_3_31: 29;
        };

        uint32_t v;
    } status;

    union {
        struct _register_bits {
            uint32_t mode: 3;
            uint32_t enable: 1;
            uint32_t reserved_4_31: 28;
        };

        uint32_t v;
    } config;
} hw_security_sha_reg_t;

typedef volatile struct {
    uint32_t config;            // 0x80
    uint32_t state;             // 0x81
    uint32_t no_prime_l;        // 0x82
    uint32_t no_prime_h;        // 0x83
    uint32_t length;            // 0x84

    uint32_t reserved_0x85;
    uint32_t reserved_0x86;
    uint32_t reserved_0x87;

    uint32_t n_rst;             // 0x88
    uint32_t n_mem_data;        // 0x89
    uint32_t e_rst;             // 0x8A
    uint32_t e_mem_data;        // 0x8B
    uint32_t m_rst;             // 0x8C
    uint32_t m_mem_data;        // 0x8D
    uint32_t r_rst;             // 0x8E
    uint32_t r_mem_data;        // 0x8F
    uint32_t t_rst;             // 0x90
    uint32_t t_mem_data;        // 0x91
    uint32_t c_rst;             // 0x92
    uint32_t c_mem_data;        // 0x93
} hw_security_rsa_reg_t;


#define hw_aes              ((volatile hw_security_aes_reg_t *) SECURITY_AES_BASE)
#define hw_sha              ((volatile hw_security_sha_reg_t *) SECURITY_SHA_BASE)
#define hw_rsa              ((volatile hw_security_rsa_reg_t *) SECURITY_RSA_BASE)
