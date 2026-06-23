#pragma once

#include <stdint.h>
#include "platform/soc.h"


#define SECURITY_BASE     (0x00806000)
#define SECURITY_AES_BASE (SECURITY_BASE + 0 * 4)
#define SECURITY_SHA_BASE (SECURITY_BASE + 0x40 * 4)
#define SECURITY_RSA_BASE (SECURITY_BASE + 0x80 * 4)


typedef volatile struct {
    uint32_t key_0to7[8];

    union {
        uint32_t v;
        struct {
            uint32_t init: 1; // [0]     start AES operation; sw sets, hw clears
            uint32_t next: 1; // [1]     continue next block
            // "auteo" is intentional: the SDK uses this misspelling instead of
            // "auto" (which is a C++ keyword). Renaming was tried and breaks
            // SDK compatibility, so it stays.
            uint32_t auteo: 1;          // [2]     auto-continue mode (misspelled in SDK; "auto" is C++ keyword)
            uint32_t int_en: 1;         // [3]     interrupt enable
            uint32_t reserved_4_31: 28; // [31:4]
        };
    } control;

    union {
        uint32_t v;
        struct {
            uint32_t ready: 1;          // [0]     1 = engine idle
            uint32_t valid: 1;          // [1]     1 = result ready
            uint32_t int_flag: 1;       // [2]     interrupt flag; W1C
            uint32_t reserved_3_31: 29; // [31:3]
        };
    } status;

    union {
        uint32_t v;
        struct {
            uint32_t encode: 1;         // [0]     0 = decrypt, 1 = encrypt
            uint32_t mode: 2;           // [2:1]   0 = ECB, 1 = CBC, 2 = CTR
            uint32_t reserved_3_31: 29; // [31:3]
        };
    } config;

    uint32_t reserved_0xB;

    uint32_t block_0to3[4];
    uint32_t result_0to3[4];
} hw_security_aes_t;

typedef volatile struct {
    uint32_t block_31to0[32];
    uint32_t digest_15to0[16];

    union {
        uint32_t v;
        struct {
            uint32_t init: 1;           // [0]     start SHA operation
            uint32_t next: 1;           // [1]     continue next block
            uint32_t reserved_2_31: 30; // [31:2]
        };
    } control;

    union {
        uint32_t v;
        struct {
            uint32_t ready: 1;          // [0]     1 = engine idle
            uint32_t valid: 1;          // [1]     1 = result ready
            uint32_t int_flag: 1;       // [2]     interrupt flag; W1C
            uint32_t reserved_3_31: 29; // [31:3]
        };
    } status;

    union {
        uint32_t v;
        struct {
            uint32_t mode: 3;           // [2:0]   0 = SHA1, 1 = SHA224, 2 = SHA256
            uint32_t enable: 1;         // [3]     SHA enable
            uint32_t reserved_4_31: 28; // [31:4]
        };
    } config;
} hw_security_sha_t;

typedef volatile struct {
    uint32_t config;     // 0x80
    uint32_t state;      // 0x81
    uint32_t no_prime_l; // 0x82
    uint32_t no_prime_h; // 0x83
    uint32_t length;     // 0x84

    uint32_t reserved_0x85;
    uint32_t reserved_0x86;
    uint32_t reserved_0x87;

    uint32_t       n_rst;      // 0x88
    uint32_t       n_mem_data; // 0x89
    uint32_t       e_rst;      // 0x8A
    uint32_t       e_mem_data; // 0x8B
    uint32_t       m_rst;      // 0x8C
    uint32_t       m_mem_data; // 0x8D
    uint32_t       r_rst;      // 0x8E
    uint32_t       r_mem_data; // 0x8F
    uint32_t       t_rst;      // 0x90
    uint32_t       t_mem_data; // 0x91
    uint32_t       c_rst;      // 0x92
    const uint32_t c_mem_data; // 0x93
} hw_security_rsa_t;

#define hw_aes ((volatile hw_security_aes_t *)SECURITY_AES_BASE)
#define hw_sha ((volatile hw_security_sha_t *)SECURITY_SHA_BASE)
#define hw_rsa ((volatile hw_security_rsa_t *)SECURITY_RSA_BASE)
