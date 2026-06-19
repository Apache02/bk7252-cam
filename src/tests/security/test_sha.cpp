#include <string.h>
#include "helpers.h"
#include "hardware/sha.h"

typedef void (*sha_fn_t)(const unsigned char *, size_t, uint8_t *);

struct sha_test_case {
    const char *input;
    const char *expected_hex;
};

struct sha_algorithm {
    const char          *name;
    sha_fn_t             fn;
    size_t               digest_size;
    const sha_test_case *cases;
    size_t               case_count;
};

static const char MSG_EMPTY[] = "";
static const char MSG_ABC[]   = "abc";
static const char MSG_FOX[]   = "The quick brown fox jumps over the lazy dog";
// FIPS 180-4 multi-block vectors (56 bytes crosses one 64-byte block; 112 bytes crosses one 128-byte block).
static const char MSG_NIST_56[]  = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
static const char MSG_NIST_112[] = "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
                                   "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";

static const sha_test_case sha1_cases[] = {
    {MSG_EMPTY, "da39a3ee5e6b4b0d3255bfef95601890afd80709"},
    {MSG_ABC, "a9993e364706816aba3e25717850c26c9cd0d89d"},
    {MSG_FOX, "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12"},
    {MSG_NIST_56, "84983e441c3bd26ebaae4aa1f95129e5e54670f1"},
};

static const sha_test_case sha224_cases[] = {
    {MSG_EMPTY, "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f"},
    {MSG_ABC, "23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7"},
    {MSG_FOX, "730e109bd7a8a32b1cb9d9a09aa2325d2430587ddbc0c38bad911525"},
    {MSG_NIST_56, "75388b16512776cc5dba5da1fd890150b0c6455cb4f58b1952522525"},
};

static const sha_test_case sha256_cases[] = {
    {MSG_EMPTY, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
    {MSG_ABC, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
    {MSG_FOX, "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592"},
    {MSG_NIST_56, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
};

static const sha_test_case sha384_cases[] = {
    {MSG_EMPTY, "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da"
                "274edebfe76f65fbd51ad2f14898b95b"},
    {MSG_ABC, "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed"
              "8086072ba1e7cc2358baeca134c825a7"},
    {MSG_FOX, "ca737f1014a48f4c0b6dd43cb177b0afd9e5169367544c494011e3317dbf9a50"
              "9cb1e5dc1e85a941bbee3d7f2afbc9b1"},
    {MSG_NIST_112, "09330c33f71147e83d192fc782cd1b4753111b173b3b05d22fa08086e3b0f712"
                   "fcc7c71a557e2db966c3e9fa91746039"},
};

static const sha_test_case sha512_cases[] = {
    {MSG_EMPTY, "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce"
                "47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e"},
    {MSG_ABC, "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
              "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"},
    {MSG_FOX, "07e547d9586f6a73f73fbac0435ed76951218fb7d0c8d788a309d785436bbb64"
              "2e93a252a954f23912547d1e8a3b5ed6e1bfd7097821233fa0538f3db854fee6"},
    {MSG_NIST_112, "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018"
                   "501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909"},
};

static const sha_test_case sha512_224_cases[] = {
    {MSG_EMPTY, "6ed0dd02806fa89e25de060c19d3ac86cabb87d6a0ddd05c333b84f4"},
    {MSG_ABC, "4634270f707b6a54daae7530460842e20e37ed265ceee9a43e8924aa"},
    {MSG_NIST_112, "23fec5bb94d60b23308192640b0c453335d664734fe40e7268674af9"},
};

static const sha_test_case sha512_256_cases[] = {
    {MSG_EMPTY, "c672b8d1ef56ed28ab87c3622c5114069bdd3ad7b8f9737498d0c01ecef0967a"},
    {MSG_ABC, "53048e2681941ef99b2e29b76b4c7dabe4c2d0c634fc6d46e0e2f13107e7af23"},
    {MSG_NIST_112, "3928e184fb8690f840da3988121d31be65cb9d3ef83ee6146feac861e19b563a"},
};

#define ALG(name_str, fn_, size_, cases_) \
    { name_str, fn_, size_, cases_, sizeof(cases_) / sizeof(cases_[0]) }

static const sha_algorithm algorithms[] = {
    ALG("SHA-1", sha1, 20, sha1_cases),
    ALG("SHA-224", sha224, 28, sha224_cases),
    ALG("SHA-256", sha256, 32, sha256_cases),
    ALG("SHA-384", sha384, 48, sha384_cases),
    ALG("SHA-512", sha512, 64, sha512_cases),
    ALG("SHA-512/224", sha512_224, 28, sha512_224_cases),
    ALG("SHA-512/256", sha512_256, 32, sha512_256_cases),
};

// ==========================================================================

static bool run_case(const sha_algorithm *alg, const sha_test_case *tc) {
    uint8_t hash[64];
    uint8_t expected[64];

    size_t in_len = strlen(tc->input);
    alg->fn(reinterpret_cast<const unsigned char *>(tc->input), in_len, hash);
    hex_decode(tc->expected_hex, expected, alg->digest_size);

    bool ok = memcmp(hash, expected, alg->digest_size) == 0;
    printf("  [%s] %s(len=%u) = ", ok ? " OK " : "FAIL", alg->name, (unsigned)in_len);
    print_hex(hash, alg->digest_size);
    if (!ok) {
        printf("\r\n         expected ");
        print_hex(expected, alg->digest_size);
    }
    printf("\r\n");
    return ok;
}

extern "C" void test_sha() {
    size_t total  = 0;
    size_t passed = 0;

    for (const sha_algorithm &alg : algorithms) {
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