#include <cstdlib>
#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include <string.h>
#include "hardware/sha.h"


static inline int hex_digit(int digit) {
    return (digit > 9 ? 'a' - 10 : '0') + digit;
}

void hash_to_string(const uint8_t *hash, size_t len, char *to) {
    int j = 0;
    for (size_t i = 0; i < len; i++) {
        to[j++] = hex_digit((hash[i] >> 4) & 0x0F);
        to[j++] = hex_digit((hash[i] >> 0) & 0x0F);
    }

    to[j] = 0;
}

int command_sha1(int argc, const char *argv[]) {
    if (argc < 2) {
        printf("Required argument\r\n");
        return 1;
    }

    const char *input = argv[1];
    auto length = strlen(input);

    auto ctx = sha1_create_context();
    sha_update(ctx, reinterpret_cast<const uint8_t *>(input), length);

    uint8_t hash[5 * sizeof(uint32_t)] = {0};
    sha_finish(ctx, hash);

    char hash_string[(sizeof(hash) * 2) + 1];
    hash_to_string(hash, sizeof(hash), hash_string);

    printf("SHA1(\"%s\") = %s\r\n", input, hash_string);

    return 0;
}

int command_sha256(int argc, const char *argv[]) {
    if (argc < 2) {
        printf("Required argument\r\n");
        return 1;
    }

    const char *input = argv[1];
    auto length = strlen(input);

    auto ctx = sha256_create_context();
    sha_update(ctx, reinterpret_cast<const uint8_t *>(input), length);

    uint8_t hash[8 * sizeof(uint32_t)] = {0};
    sha_finish(ctx, hash);

    char hash_string[(sizeof(hash) * 2) + 1];
    hash_to_string(hash, sizeof(hash), hash_string);

    printf("SHA256(\"%s\") = %s\r\n", input, hash_string);

    return 0;
}

// int command_sha224(int argc, const char *argv[]) {
//     if (argc < 2) {
//         printf("Required argument\r\n");
//         return 1;
//     }
//
//     const char *input = argv[1];
//     auto length = strlen(input);
//
//     auto ctx = sha224_create_context();
//     sha_update(ctx, reinterpret_cast<const uint8_t *>(input), length);
//
//     uint8_t hash[7 * sizeof(uint32_t)] = {0};
//     sha_finish(ctx, hash);
//
//     char hash_string[(sizeof(hash) * 2) + 1];
//     hash_to_string(hash, sizeof(hash), hash_string);
//
//     printf("SHA224(\"%s\") = %s\r\n", input, hash_string);
//
//     return 0;
// }
