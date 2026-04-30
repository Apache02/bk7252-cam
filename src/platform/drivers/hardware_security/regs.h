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

typedef struct {
    volatile uint32_t ptr_reset;
    volatile uint32_t mem_data;
} cyclic_memory_bank_t;

typedef volatile struct {
    union {
        struct _register_bits {
            uint32_t init: 1;
            uint32_t next: 1;
            uint32_t enable: 1;        // hypothesis, maybe its encode/decode flag
            uint32_t reserved_3_31: 29;
        };

        uint32_t v;
    } control;

    union {
        struct _register_bits {
            uint32_t ready: 1;
            uint32_t enabled: 1;        // hypothesis, maybe its valid flag
            uint32_t int_flag: 1;       // hypothesis, not tested
            uint32_t reserved_3_31: 29;
        };

        uint32_t v;
    } status;

    // 0x82, 0x83
    uint32_t no_prime_l;
    uint32_t no_prime_h;

    // 0x84 — operation size / mode selector (purpose still under investigation).
    //
    // Field layout:
    //   bit  0       — ignored (writing 0/1 produces identical behavior)
    //   bits 1..6    — significant: this 6-bit field controls operation size
    //                  and dominates compute duration (~quadratic growth)
    //   bit  7       — ignored for hardware semantics, but readback returns
    //                  whatever was written
    //   bits 8..31   — not stored (writing 0x100 reads back 0x00)
    //
    // The exact unit encoded in bits [1..6] is unknown. Originally hypothesized
    // as a mode selector (16/64/128 = RSA-512/2048/4096) but no valid RSA
    // result has been produced for any value tested. The leading hypothesis
    // is that this is operand length in some unit (words / half-words / bits),
    // possibly with bit 0 acting as an alignment requirement that the hardware
    // silently masks off.
    uint32_t config;

    uint32_t reserved_0x85;
    uint32_t reserved_0x86;
    uint32_t reserved_0x87;

    // Memory banks: 128 32-bit words each, accessed via *_rst (pointer
    // reset to 0) and *_mem_data (RW with auto-increment, wraps at 128).
    cyclic_memory_bank_t n;      // RW | used as mod N on init | 0 and 1 => infinite cycle
    cyclic_memory_bank_t e;      // RW | rounds count on init  | 0 => infinite cycle
    cyclic_memory_bank_t m;      // RW | used as M*T on init   |
    cyclic_memory_bank_t r;      // RW | unknown
    cyclic_memory_bank_t t;      // RW | used as M*T on init
    cyclic_memory_bank_t c;      // RO bank | result?

    // M = f(no_prime_l, no_prime_h, E, R, M*T) mod N
    // C = g(M, E, T) mod N

    // C(R) = 86 * R^4 mod 101                  (when N=101, E=3, M=7, T=0x44)
    // M[0] = 10·T mod 101                      (when N=101, E=3, M=7, R=1)
    // C[0] = 95 · M_in^3 · T^3 · R^4 mod N     (when N=101, E=3)
    // M[0] = 88 · M_in · T · R^2 mod N         (when N=101, E=3, 88 = R_mont mod N)
    // C[0] = M^E · T^E · R^(2^msb(E)) · 2^(44E - 28·2^msb(E)) mod N


} hw_security_rsa_reg_t;


#define hw_aes              ((volatile hw_security_aes_reg_t *) SECURITY_AES_BASE)
#define hw_sha              ((volatile hw_security_sha_reg_t *) SECURITY_SHA_BASE)
#define hw_rsa              ((volatile hw_security_rsa_reg_t *) SECURITY_RSA_BASE)
