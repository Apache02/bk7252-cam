#include <string.h>
#include "helpers.h"
#include "hardware/aes.h"

typedef int (*aes_fn_t)(const uint8_t *, const uint8_t *, uint8_t *, size_t);

struct aes_test_case {
    const char *name;
    const char *key_hex;      // 16 / 24 / 32 bytes
    const char *input_hex;    // multiple of 16 bytes
    const char *expected_hex; // same length as input
};

struct aes_algorithm {
    const char          *name;
    aes_fn_t             fn;
    size_t               key_size; // bytes
    const aes_test_case *cases;
    size_t               case_count;
};

// Single-block vectors: FIPS 197 Appendix C (pt = 00112233...eeff).
// Multi-block vectors: NIST SP 800-38A §F.1 ECB examples (4 blocks = 64 B).

static const char PT_FIPS[]  = "00112233445566778899aabbccddeeff";
static const char PT_NIST4[] = "6bc1bee22e409f96e93d7e117393172a"
                               "ae2d8a571e03ac9c9eb76fac45af8e51"
                               "30c43d2be049dcf10c8a8e6dc1e2b1fe"
                               "f69f2445df4f9b17ad2b417be66c4110";

// ---- AES-128 encrypt cases ----
static const aes_test_case aes128_enc_cases[] = {
    {
        "FIPS-197 single block",
        "000102030405060708090a0b0c0d0e0f",
        "00112233445566778899aabbccddeeff",
        "69c4e0d86a7b0430d8cdb78070b4c55a",
    },
    {
        "NIST SP 800-38A 4 blocks",
        "2b7e151628aed2a6abf7158809cf4f3c",
        PT_NIST4,
        "3ad77bb40d7a3660a89ecaf32466ef97"
        "f5d3d58503b9699de785895a96fdbaaf"
        "d29871e8eff77ffa62790dcb133636c9"
        "f75fda78f96404c212b72c5465155aad",
    },
};

// ---- AES-128 decrypt cases (same vectors, swapped) ----
static const aes_test_case aes128_dec_cases[] = {
    {
        "FIPS-197 single block",
        "000102030405060708090a0b0c0d0e0f",
        "69c4e0d86a7b0430d8cdb78070b4c55a",
        "00112233445566778899aabbccddeeff",
    },
    {
        "NIST SP 800-38A 4 blocks",
        "2b7e151628aed2a6abf7158809cf4f3c",
        "3ad77bb40d7a3660a89ecaf32466ef97"
        "f5d3d58503b9699de785895a96fdbaaf"
        "d29871e8eff77ffa62790dcb133636c9"
        "f75fda78f96404c212b72c5465155aad",
        PT_NIST4,
    },
};

// ---- AES-192 ----
static const aes_test_case aes192_enc_cases[] = {
    {
        "FIPS-197 single block",
        "000102030405060708090a0b0c0d0e0f1011121314151617",
        "00112233445566778899aabbccddeeff",
        "dda97ca4864cdfe06eaf70a0ec0d7191",
    },
    {
        "NIST SP 800-38A 4 blocks",
        "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
        PT_NIST4,
        "bd334f1d6e45f25ff712a214571fa5cc"
        "974104846d0ad3ad7734ecb3ecee4eef"
        "3e5bf90492f2b098755799bfbe2f6057"
        "a396a7f05fbfa7f7764b789ea61c3a2e",
    },
};

static const aes_test_case aes192_dec_cases[] = {
    {
        "FIPS-197 single block",
        "000102030405060708090a0b0c0d0e0f1011121314151617",
        "dda97ca4864cdfe06eaf70a0ec0d7191",
        "00112233445566778899aabbccddeeff",
    },
    {
        "NIST SP 800-38A 4 blocks",
        "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
        "bd334f1d6e45f25ff712a214571fa5cc"
        "974104846d0ad3ad7734ecb3ecee4eef"
        "3e5bf90492f2b098755799bfbe2f6057"
        "a396a7f05fbfa7f7764b789ea61c3a2e",
        PT_NIST4,
    },
};

// ---- AES-256 ----
static const aes_test_case aes256_enc_cases[] = {
    {
        "FIPS-197 single block",
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
        "00112233445566778899aabbccddeeff",
        "8ea2b7ca516745bfeafc49904b496089",
    },
    {
        "NIST SP 800-38A 4 blocks",
        "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4",
        PT_NIST4,
        "f3eed1bdb5d2a03c064b5a7e3db181f8"
        "591ccb10d410ed26dc5ba74a31362870"
        "02501743c22457162ee44a08c9f43900"
        "e14a455a2c1807e37a28095e0fb36abb",
    },
};

static const aes_test_case aes256_dec_cases[] = {
    {
        "FIPS-197 single block",
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
        "8ea2b7ca516745bfeafc49904b496089",
        "00112233445566778899aabbccddeeff",
    },
    {
        "NIST SP 800-38A 4 blocks",
        "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4",
        "f3eed1bdb5d2a03c064b5a7e3db181f8"
        "591ccb10d410ed26dc5ba74a31362870"
        "02501743c22457162ee44a08c9f43900"
        "e14a455a2c1807e37a28095e0fb36abb",
        PT_NIST4,
    },
};

#define ALG(name_str, fn_, key_size_, cases_) \
    { name_str, fn_, key_size_, cases_, sizeof(cases_) / sizeof(cases_[0]) }

static const aes_algorithm aes_algorithms[] = {
    ALG("AES-128 encrypt", aes128_encrypt, 16, aes128_enc_cases),
    ALG("AES-192 encrypt", aes192_encrypt, 24, aes192_enc_cases),
    ALG("AES-256 encrypt", aes256_encrypt, 32, aes256_enc_cases),
    ALG("AES-128 decrypt", aes128_decrypt, 16, aes128_dec_cases),
    ALG("AES-192 decrypt", aes192_decrypt, 24, aes192_dec_cases),
    ALG("AES-256 decrypt", aes256_decrypt, 32, aes256_dec_cases),
};

// ==========================================================================

static bool run_case(const aes_algorithm *alg, const aes_test_case *tc) {
    uint8_t key[32];
    uint8_t input[64];
    uint8_t expected[64];
    uint8_t output[64];

    size_t data_len = strlen(tc->input_hex) / 2;
    hex_decode(tc->key_hex, key, alg->key_size);
    hex_decode(tc->input_hex, input, data_len);
    hex_decode(tc->expected_hex, expected, data_len);

    int  rc = alg->fn(key, input, output, data_len);
    bool ok = (rc == 0) && (memcmp(output, expected, data_len) == 0);

    printf("  [%s] %s [%s] len=%u = ", ok ? " OK " : "FAIL", alg->name, tc->name, (unsigned)data_len);
    print_hex(output, data_len);
    if (!ok) {
        printf("\r\n         expected ");
        print_hex(expected, data_len);
    }
    printf("\r\n");
    return ok;
}

extern "C" void test_aes() {
    size_t total  = 0;
    size_t passed = 0;

    for (const aes_algorithm &alg : aes_algorithms) {
        printf("== %s ==\r\n", alg.name);
        for (size_t i = 0; i < alg.case_count; i++) {
            total++;
            if (run_case(&alg, &alg.cases[i])) {
                passed++;
            }
        }
    }

    printf("\r\n%u / %u passed\r\n\r\n", (unsigned)passed, (unsigned)total);
}