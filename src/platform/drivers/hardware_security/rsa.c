#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "regs.h"

#define DEBUG_NAME "RSA"
#include "shell/console_colors.h"
#include "shell/console_debug.h"

#define count_of(x)     (sizeof(x) / sizeof(x[0]))


// ============================================================================
//  Working hypothesis
// ----------------------------------------------------------------------------
//  config is a CONTROL register, not a mode selector:
//      bit0,bit1 = 2-bit phase command (self-clearing trigger)
//                  values 01 / 10 / 11 = compute_C / compute_T / compute_R
//                  (exact mapping unknown — stage 4 brute-forces it)
//      bit2      = enable (sticky, must be held during all three phases)
//
//  length = operand size in 32-bit words: 16 / 64 / 128 = RSA-512/2048/4096.
//           Mode is implied by length, no separate mode field exists.
//
//  state.bit0 = ready/done; state.bit1 = mirror of config.bit2 (enable echo).
//
//  Test vector (RSA-512, 16 words, LSW-first):
//      N    = 0xFFFFFFFFFFFFFFC5      (a 64-bit prime; rest zero)
//      E    = 0x10001                  (65537)
//      M    = 2
//      N0'  = 0xCBEEA4E1A08AD8F3       (-N^-1 mod 2^64, Montgomery const)
//      C    = 0x858AA0135BE1           (= 2^512 mod N, expected after phase 1)
//      R    = 0x3427C9ACA4F7AF13       (= M^E mod N, expected after phase 3)
// ============================================================================


// ----- test vector ---------------------------------------------------------

// #define LEN 16
//
// static const uint32_t TEST_N[LEN] = {
//     0xFFFFFFC5u, 0xFFFFFFFFu,  // rest zero
// };
// static const uint32_t TEST_E[LEN] = { 0x00010001u };
// static const uint32_t TEST_M[LEN] = { 0x00000002u };
//
// static const uint32_t EXPECTED_C[LEN] = { 0xA0135BE1u, 0x0000858Au };
// static const uint32_t EXPECTED_R[LEN] = { 0xA4F7AF13u, 0x3427C9ACu };
//
// #define N0_PRIME_L  0xA08AD8F3u
// #define N0_PRIME_H  0xCBEEA4E1u


// ----- low-level helpers ---------------------------------------------------

__unused static void bank_write(volatile cyclic_memory_bank_t *bank, const uint32_t *words, int nwords) {
    bank->ptr_reset = 0;
    for (int i = 0; i < nwords; i++) bank->mem_data = words[i];
}

__unused static void bank_fill(volatile cyclic_memory_bank_t *bank, const uint32_t value, int nwords) {
    bank->ptr_reset = 0;
    for (int i = 0; i < nwords; i++) bank->mem_data = value;
}

__unused static void bank_fill_seq(volatile cyclic_memory_bank_t *bank, int nwords) {
    bank->ptr_reset = 0;
    for (int i = 0; i < nwords; i++) bank->mem_data = i;
}

__unused static void bank_put_be(volatile cyclic_memory_bank_t *bank, uint32_t number) {
    bank->ptr_reset = 0;
    bank->mem_data = __builtin_bswap32(number);
    for (int i = 1; i < 128; i++) bank->mem_data = 0;
}

__unused static void bank_put_le(volatile cyclic_memory_bank_t *bank, uint32_t number) {
    bank->ptr_reset = 0;
    bank->mem_data = number;
    for (int i = 1; i < 128; i++) bank->mem_data = 0;
}

__unused static void bank_zero(volatile cyclic_memory_bank_t *bank, const uint32_t value, int nwords) {
    bank_fill(bank, 0, nwords);
}

__unused static void bank_read(volatile cyclic_memory_bank_t *bank, uint32_t *out, int nwords) {
    bank->ptr_reset = 0;
    for (int i = 0; i < nwords; i++) {
        out[i] = bank->mem_data;
    }
}

__unused static bool is_every_equals(uint32_t *ptr, int count, uint32_t value) {
    for (int i = 0; i < count; i++) {
        if (ptr[i] != value) return false;
    }
    return true;
}

__unused static void dump_bank(volatile cyclic_memory_bank_t *bank, const char *name) {
    // max 128
    uint32_t *buf = malloc(sizeof(uint32_t) * 128);
    bank_read(bank, buf, 128);

    printf("%s:\r\n", name);
    if (is_every_equals(&buf[0], 128, 0)) {
        printf("  ... all %d is %08lx\r\n", 128, 0x00000000L);
    } else {
        for (int i = 0; i < 128;) {
            printf("  ");
            for (int j = 0; j < 8; j++) {
                printf("%08lx ", buf[i]);
                i++;
            }
            printf("\r\n");

            int count_left = 128 - i;
            if (count_left > 0 && is_every_equals(&buf[i], count_left, buf[i])) {
                printf("  ... rest %d is %08lx\r\n", count_left, buf[i]);
                break;
            }
        }
    }

    free(buf);
}

#define POLL_MAX        INT16_MAX

__unused static int wait_ready(void) {
    for (int i = 0; i < POLL_MAX; i++) {
        if (hw_rsa->status.ready) return i;
    }

    return -1;
}

__unused static int wait_status_change(uint32_t pre) {
    for (int i = 0; i < POLL_MAX; i++) {
        if (hw_rsa->status.v != pre) return i;
    }

    return -1;
}


// ===========================================================================
//  Actions
// ===========================================================================

void action_reset() {
    bank_put_le(&hw_rsa->n, 0xFFFFFFC5);
    bank_put_le(&hw_rsa->e, 0x10001);
    bank_fill(&hw_rsa->m, 0, 128);
    bank_fill(&hw_rsa->r, 0, 128);
    bank_fill(&hw_rsa->t, 0, 128);

    hw_rsa->no_prime_l = 0xA08AD8F3u;
    hw_rsa->no_prime_h = 0xCBEEA4E1u;
    hw_rsa->config = 0x7E;

    LOG_REG(hw_rsa->no_prime_l);
    LOG_REG(hw_rsa->no_prime_h);
    LOG_REG(hw_rsa->config);

    hw_rsa->control.enable = 1;
    hw_rsa->control.init = 1;

    dump_bank(&hw_rsa->c, "C");
    dump_bank(&hw_rsa->m, "M");
}

void action_test_A(uint32_t config, uint32_t n, uint32_t e, uint32_t m, uint32_t r, uint32_t t) {
    hw_rsa->no_prime_l = 0xA08AD8F3u;
    hw_rsa->no_prime_h = 0xCBEEA4E1u;

    bank_put_le(&hw_rsa->n, n);
    bank_put_le(&hw_rsa->e, e);
    bank_put_le(&hw_rsa->m, m);
    bank_put_le(&hw_rsa->r, r);
    bank_put_le(&hw_rsa->t, t);

    hw_rsa->config = config;
    LOG_I("config 0x%02lx | n = 0x%08lx | e = 0x%08lx | m = 0x%08lx | r = 0x%08lx | t = 0x%08lx", config, n, e, m, r,
          t);

    hw_rsa->control.enable = 0;
    hw_rsa->control.init = 1;
    LOG_I("wait=%d", wait_ready());

    dump_bank(&hw_rsa->c, "C");
    dump_bank(&hw_rsa->m, "M");
}


// ===========================================================================
//  Універсальна тестова функція
// ===========================================================================

// Один прогон init з усіма параметрами, рядок таблиці
static void run_case(uint32_t config,
                     uint32_t no_prime_l, uint32_t no_prime_h,
                     uint32_t n, uint32_t e,
                     uint32_t m, uint32_t r, uint32_t t) {
    hw_rsa->no_prime_l = no_prime_l;
    hw_rsa->no_prime_h = no_prime_h;
    bank_put_le(&hw_rsa->n, n);
    bank_put_le(&hw_rsa->e, e);
    bank_put_le(&hw_rsa->m, m);
    bank_put_le(&hw_rsa->r, r);
    bank_put_le(&hw_rsa->t, t);

    hw_rsa->config = config;
    hw_rsa->control.enable = 0;
    hw_rsa->control.init = 1;
    int w = wait_ready();

    hw_rsa->c.ptr_reset = 0;
    uint32_t c0 = hw_rsa->c.mem_data;
    hw_rsa->m.ptr_reset = 0;
    uint32_t m0 = hw_rsa->m.mem_data;

    printf("  %02lx | %08lx %08lx | %08lx | %08lx | %08lx | %08lx | %08lx | %5d | %08lx | %08lx\r\n",
           (unsigned long) config,
           (unsigned long) no_prime_l, (unsigned long) no_prime_h,
           (unsigned long) n, (unsigned long) e,
           (unsigned long) m, (unsigned long) r, (unsigned long) t,
           w,
           (unsigned long) c0, (unsigned long) m0);
}

static void run_case_print_table_divider(void) {
    printf("  ---+-------------------+----------+----------+----------+----------+----------+-------+----------+---------\r\n");
}

static void run_case_print_table_head(void) {
    printf("\r\n");
    printf("  cf | no_prime \"_l _h\"  | N        | E        | M        | R        | T        | wait  | C[0]     | M[0]\r\n");
    run_case_print_table_divider();
}

void action_scan_pack_e() {
    int i;

    run_case_print_table_head();

    // for (i = 1; i < 16; i++) {
    //     run_case(0x02, 0x65fdf5cd, 0xd7340414, 251, i, 1, 2, 1);
    // }
    for (i = 1; i < 16; i++) {
        run_case(0x02, 0x83cd4e93, 0xc5b3f5dc, 101, i, 7, 2, 1);
    }
    // for (i = 0; i < 32; i++) {
    //     run_case(0x02, 0xA08AD8F3u, 0xCBEEA4E1u, 99, 1<<i, 1, 2, 2);
    // }
    // int v = 0x80000000;
    // for (i = 0; i < 32; i++) {
    //     v = v | v >> 1;
    //     run_case(0x02, 0xA08AD8F3u, 0xCBEEA4E1u, 99, v, 1, 2, 2);
    // }
    // run_case(0x02, 0xA08AD8F3u, 0xCBEEA4E1u, 99, 0xAAAAAAAA, 1, 2, 2);
    // run_case(0x02, 0xA08AD8F3u, 0xCBEEA4E1u, 99, 0x55555555, 1, 2, 2);
}

void action_scan_pack_t() {
    int i;

    run_case_print_table_head();

    for (i = 0; i < 32; i++) {
        run_case(0x02, 0x83cd4e93, 0xc5b3f5dc, 101, 3, 7, 1, i);
    }
    // int v = 0x80000000;
    // for (i = 0; i < 32; i++) {
    //     v = v | v >> 1;
    //     run_case(0x02, 0xA08AD8F3u, 0xCBEEA4E1u, 99, 1, 1, 2, v);
    // }
    // run_case(0x02, 0xA08AD8F3u, 0xCBEEA4E1u, 99, 1, 1, 2, 0xAAAAAAAA);
    // run_case(0x02, 0xA08AD8F3u, 0xCBEEA4E1u, 99, 1, 1, 2, 0x55555555);
}

void action_scan_pack_r() {
    int i;

    run_case_print_table_head();

    for (i = 0; i < 32; i++) {
        run_case(0x02, 0x83cd4e93, 0xc5b3f5dc, 101, 3, 7, i, 0x00000044);
    }
    // for (i = 0; i < 32; i++) {
    //     run_case(0x02, 0xA08AD8F3u, 0xCBEEA4E1u, 100, 1, 2, 1<<i, 2);
    // }
    // int v = 0x80000000;
    // for (i = 0; i < 32; i++) {
    //     v = v | v >> 1;
    //     run_case(0x02, 0xA08AD8F3u, 0xCBEEA4E1u, 100, 1, 2, v, 2);
    // }
    // run_case(0x02, 0xA08AD8F3u, 0xCBEEA4E1u, 100, 1, 2, 0xAAAAAAAA, 2);
    // run_case(0x02, 0xA08AD8F3u, 0xCBEEA4E1u, 100, 1, 2, 0x55555555, 2);
}

void action_scan_pack_m() {
    int i;

    run_case_print_table_head();

    for (i = 0; i < 32; i++) {
        run_case(0x02, 0x83cd4e93, 0xc5b3f5dc, 101, 3, i, 1, 1);
    }
    // for (i = 0; i < 32; i++) {
    //     run_case(0x02, 0xA08AD8F3u, 0xCBEEA4E1u, 100, 1, 2, 1<<i, 2);
    // }
    // int v = 0x80000000;
    // for (i = 0; i < 32; i++) {
    //     v = v | v >> 1;
    //     run_case(0x02, 0xA08AD8F3u, 0xCBEEA4E1u, 100, 1, 2, v, 2);
    // }
    // run_case(0x02, 0xA08AD8F3u, 0xCBEEA4E1u, 100, 1, 2, 0xAAAAAAAA, 2);
    // run_case(0x02, 0xA08AD8F3u, 0xCBEEA4E1u, 100, 1, 2, 0x55555555, 2);
}

void action_test_rsa() {
    unsigned int public_e = 3,
                 public_n = 33,
                 private_d = 7;

    printf("== RSA TEST\r\n");
    printf("  public key (e; n)         = (%u; %u)\r\n", public_e, public_n);
    printf("  private key (d)           = (%u)\r\n", private_d);

    // no_prime = -n^(-1) mod 2^64
    // R = 2^28 mod n
    // T = 2^(-44) mod n
    unsigned long computed_no_prime_l = 0xc1f07c1f;
    unsigned long computed_no_prime_h = 0xf07c1f07;
    unsigned long computed_t = 31;
    unsigned long computed_r = 25;

    printf("  computed no_prime (l; h)  = (0x%08lx; 0x%08lx)\r\n", computed_no_prime_l, computed_no_prime_h);
    printf("  computed T                = 0x%08lx\r\n", computed_t);
    printf("  computed R                = 0x%08lx\r\n", computed_r);
    printf("\r\n");

    unsigned long message = 4;
    unsigned long encrypted = 31;
    unsigned long value;

    hw_rsa->config = 2;
    hw_rsa->no_prime_l = computed_no_prime_l;
    hw_rsa->no_prime_h = computed_no_prime_h;
    bank_put_le(&hw_rsa->t, computed_t);
    bank_put_le(&hw_rsa->r, computed_r);

    hw_rsa->control.enable = 1;

    printf("+ encryption\r\n");
    printf("  message = %lu\r\n", message);
    bank_put_le(&hw_rsa->n, public_n);
    bank_put_le(&hw_rsa->e, public_e);
    bank_put_le(&hw_rsa->m, message);
    hw_rsa->control.init = 1;
    printf("    waited %d iterations\r\n", wait_ready());

    hw_rsa->c.ptr_reset = 0;
    value = hw_rsa->c.mem_data;
    printf("  encrypted = %lu | expected = %lu - [%s]\r\n", value, encrypted, value == encrypted ? " OK " : COLOR_RED("FAIL"));


    printf("+ decryption\r\n");
    printf("  encrypted = %lu\r\n", encrypted);
    bank_put_le(&hw_rsa->n, public_n);
    bank_put_le(&hw_rsa->e, private_d);
    bank_put_le(&hw_rsa->m, encrypted);
    hw_rsa->control.init = 1;
    printf("    waited %d iterations\r\n", wait_ready());

    hw_rsa->c.ptr_reset = 0;
    value = hw_rsa->c.mem_data;
    printf("  decrypted = %lu | expected = %lu - [%s]\r\n", value, message, value == message ? " OK " : COLOR_RED("FAIL"));
}

// ===========================================================================
//  Entry point
// ===========================================================================

void rsa_explore(const char *action) {
    if (strcmp(action, "reset") == 0) {
        action_reset();
        return;
    }

    if (strcmp(action, "setup") == 0) {
        hw_rsa->no_prime_l = 0xA08AD8F3u;
        hw_rsa->no_prime_h = 0xCBEEA4E1u;
        bank_put_le(&hw_rsa->n, 0xFFFFFFC5u);
        bank_put_le(&hw_rsa->e, 0x10001u);
        bank_put_le(&hw_rsa->m, 2);
        bank_put_le(&hw_rsa->r, 2);
        bank_put_le(&hw_rsa->t, 2);
        hw_rsa->config = 0x02;
        hw_rsa->control.enable = 0;
        LOG_I("setup done: M=R=T=2, N=0xFFFFFFC5, E=0x10001, config=0x02");
        return;
    }

    if (strcmp(action, "enable") == 0) {
        hw_rsa->control.enable = 1;
        LOG_REG(hw_rsa->control.v);
        return;
    }

    if (strcmp(action, "disable") == 0) {
        hw_rsa->control.enable = 0;
        LOG_REG(hw_rsa->control.v);
        return;
    }

    if (strcmp(action, "init") == 0) {
        hw_rsa->control.init = 1;
        LOG_I("waited %d iterations after init", wait_ready());
        return;
    }

    if (strcmp(action, "next") == 0) {
        hw_rsa->control.next = 1;
        LOG_I("waited %d iterations after next", wait_ready());
        return;
    }

    if (strcmp(action, "dump") == 0) {
        LOG_REG(hw_rsa->config);
        LOG_REG(hw_rsa->status.v);
        LOG_REG(hw_rsa->no_prime_l);
        LOG_REG(hw_rsa->no_prime_h);
        dump_bank(&hw_rsa->n, "N");
        dump_bank(&hw_rsa->e, "E");
        dump_bank(&hw_rsa->m, "M");
        dump_bank(&hw_rsa->r, "R");
        dump_bank(&hw_rsa->t, "T");
        dump_bank(&hw_rsa->c, "C");
        return;
    }

    if (strncmp(action, "dump_", 5) == 0 && strlen(action) == 6) {
        switch (action[5]) {
            case 'N':
            case 'n':
                dump_bank(&hw_rsa->n, "N");
                break;
            case 'E':
            case 'e':
                dump_bank(&hw_rsa->e, "E");
                break;
            case 'M':
            case 'm':
                dump_bank(&hw_rsa->m, "M");
                break;
            case 'R':
            case 'r':
                dump_bank(&hw_rsa->r, "R");
                break;
            case 'T':
            case 't':
                dump_bank(&hw_rsa->t, "T");
                break;
            case 'C':
            case 'c':
                dump_bank(&hw_rsa->c, "C");
                break;
        }
        return;
    }

    if (strncmp(action, "scan_", 5) == 0 && strlen(action) == 6) {
        switch (action[5]) {
            case 'N':
            case 'n':
                LOG_I("no scan n");
                break;
            case 'E':
            case 'e':
                action_scan_pack_e();
                break;
            case 'M':
            case 'm':
                action_scan_pack_m();
                break;
            case 'R':
            case 'r':
                action_scan_pack_r();
                break;
            case 'T':
            case 't':
                action_scan_pack_t();
                break;
        }
        return;
    }

    if (strcmp(action, "test") == 0) {
        action_test_rsa();
        return;
    }

    if (strcmp(action, "A") == 0) {
        action_test_A(0x02, 100, 0x10001, 1, 0, 1);
        action_test_A(0x02, 100, 0x10001, 1, 1, 1);
        action_test_A(0x02, 100, 0x10001, 1, 16, 1);
        action_test_A(0x02, 100, 0x10001, 1, 0xFFFFFFFF, 1);

        action_test_A(0x02, 100, 0x10001, 1, 0, 16);
        action_test_A(0x02, 100, 0x10001, 1, 1, 16);
        action_test_A(0x02, 100, 0x10001, 1, 16, 16);
        action_test_A(0x02, 100, 0x10001, 1, 0xFFFFFFFF, 16);

        action_test_A(0x02, 100, 0x10001, 1, 0, 0xFFFFFFFF);
        action_test_A(0x02, 100, 0x10001, 1, 1, 0xFFFFFFFF);
        action_test_A(0x02, 100, 0x10001, 1, 16, 0xFFFFFFFF);
        action_test_A(0x02, 100, 0x10001, 1, 0xFFFFFFFF, 0xFFFFFFFF);

        return;
    }
}
