#include <cstdlib>

#include "shell/commands_beken.h"
#include <stdio.h>
#include <string.h>
#include "hardware/sha.h"

static inline int hex_digit(int digit) { return (digit > 9 ? 'a' - 10 : '0') + digit; }

void hash_to_string(const uint8_t *hash, size_t len, char *to) {
    int j = 0;
    for (size_t i = 0; i < len; i++) {
        to[j++] = hex_digit((hash[i] >> 4) & 0x0F);
        to[j++] = hex_digit((hash[i] >> 0) & 0x0F);
    }

    to[j] = 0;
}

typedef void (*sha_fn_t)(const unsigned char *, size_t, uint8_t *);

static int command_sha_general(const char *name, size_t bits, sha_fn_t fn, int argc, const char *argv[]) {
    if (argc < 2) {
        printf("Required argument\r\n");
        return 1;
    }

    const int   hash_length = bits / 8;
    const char *input       = argv[1];
    uint8_t    *hash        = new uint8_t[hash_length];
    memset(hash, 0, hash_length);

    fn(reinterpret_cast<const unsigned char *>(input), strlen(input), hash);

    char *hash_string = new char[(hash_length * 2) + 1];
    hash_to_string(hash, hash_length, hash_string);

    printf("%s(\"%s\") = %s\r\n", name, input, hash_string);

    delete[] hash;
    delete[] hash_string;

    return 0;
}

int command_sha1(int argc, const char *argv[]) { return command_sha_general("SHA1", 160, sha1, argc, argv); }

int command_sha224(int argc, const char *argv[]) { return command_sha_general("SHA224", 224, sha224, argc, argv); }

int command_sha256(int argc, const char *argv[]) { return command_sha_general("SHA256", 256, sha256, argc, argv); }

int command_sha512(int argc, const char *argv[]) { return command_sha_general("SHA512", 512, sha512, argc, argv); }
