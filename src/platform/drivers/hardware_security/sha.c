#include "regs.h"
#include "hardware/sha.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef enum {
    STEP1 = 0,
    STEP2 = 1,
} SHA_STEP;

typedef enum {
    SHA1 = 0,
    SHA224 = 2,
    SHA256 = 3,
    SHA512_224 = 4,
    SHA512_256 = 5,
    SHA384 = 6,
    SHA512 = 7
} SHA_MODE;

typedef struct {
    SHA_MODE mode;
    SHA_STEP step;
    uint32_t block_size;         // 64 for SHA-1/224/256, 128 for SHA-384/512
    uint64_t total;              // bytes accumulated so far
    uint32_t buffer_len;         // bytes currently in buffer
    unsigned char buffer[128];   // max block size
} sha_context_t;


static inline void _wait_ready(void) {
    while (!hw_sha->status.ready) ;
}

static void _process_block(sha_context_t *c) {
    // Block layout (empirical, BK7252):
    //   message word 0 at the tail of the register file — at block_31to0[(32 - block_words) + 0].
    //   For SHA-1/224/256 (16 words): block_31to0[16..31]; for SHA-384/512 (32 words): block_31to0[0..31].
    int block_words = (int) (c->block_size / 4);
    int base = 32 - block_words;

    for (int i = 0; i < block_words; i++) {
        uint32_t w = ((uint32_t) c->buffer[i * 4 + 0] << 24)
                     | ((uint32_t) c->buffer[i * 4 + 1] << 16)
                     | ((uint32_t) c->buffer[i * 4 + 2] << 8)
                     | ((uint32_t) c->buffer[i * 4 + 3]);
        hw_sha->block_31to0[base + i] = w;
    }

    // INIT (bit 0) for the first block: resets state to H0 and processes.
    // NEXT (bit 1) for subsequent blocks: continues from current state.
    if (c->step == STEP1) {
        hw_sha->control.init = 1;
        c->step = STEP2;
    } else {
        hw_sha->control.next = 1;
    }
    // hw_sha->control.v = (c->step == STEP1) ? (1u << 0) : (1u << 1);
    _wait_ready();

    // if (c->step == STEP1) c->step = STEP2;
}

void *sha_create_context(SHA_MODE mode) {
    sha_context_t *ctx = (sha_context_t *) malloc(sizeof(sha_context_t));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(sha_context_t));
    ctx->mode = mode;
    ctx->step = STEP1;
    ctx->block_size = (mode <= SHA256) ? 64 : 128;

    hw_sha->config.enable = 1;
    hw_sha->config.mode = mode;

    return ctx;
}

void *sha1_create_context() {
    return sha_create_context(SHA1);
}

void *sha256_create_context() {
    return sha_create_context(SHA256);
}

// void *sha224_create_context() {
//     return sha_create_context(SHA224);
// }

void sha_destroy_context(void *ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(sha_context_t));
    free(ctx);
}

void sha_update(void *ctx, const unsigned char *input, const size_t length) {
    sha_context_t *c = (sha_context_t *) ctx;
    size_t remaining = length;

    c->total += length;

    while (remaining > 0) {
        size_t fill = c->block_size - c->buffer_len;
        size_t chunk = (remaining < fill) ? remaining : fill;
        memcpy(c->buffer + c->buffer_len, input, chunk);
        c->buffer_len += chunk;
        input += chunk;
        remaining -= chunk;

        if (c->buffer_len == c->block_size) {
            _process_block(c);
            c->buffer_len = 0;
        }
    }
}

void sha_finish(void *ctx, uint8_t *sum) {
    sha_context_t *c = (sha_context_t *) ctx;
    uint64_t total_bits = c->total * 8;

    // Length field: 8 bytes for SHA-1/224/256, 16 bytes for SHA-384/512.
    size_t length_size = (c->block_size == 128) ? 16 : 8;
    size_t padding_end = c->block_size - length_size;

    // Append 0x80 terminator.
    c->buffer[c->buffer_len++] = 0x80;

    // If not enough room for the length field in this block, flush a zero-padded block.
    if (c->buffer_len > padding_end) {
        memset(c->buffer + c->buffer_len, 0, c->block_size - c->buffer_len);
        _process_block(c);
        c->buffer_len = 0;
    }
    memset(c->buffer + c->buffer_len, 0, padding_end - c->buffer_len);
    c->buffer_len = padding_end;

    // For SHA-384/512 the length is 128-bit; high 64 bits are zero since c->total fits in 64 bits.
    if (length_size == 16) {
        memset(c->buffer + c->buffer_len, 0, 8);
        c->buffer_len += 8;
    }
    c->buffer[c->buffer_len + 0] = (uint8_t) (total_bits >> 56);
    c->buffer[c->buffer_len + 1] = (uint8_t) (total_bits >> 48);
    c->buffer[c->buffer_len + 2] = (uint8_t) (total_bits >> 40);
    c->buffer[c->buffer_len + 3] = (uint8_t) (total_bits >> 32);
    c->buffer[c->buffer_len + 4] = (uint8_t) (total_bits >> 24);
    c->buffer[c->buffer_len + 5] = (uint8_t) (total_bits >> 16);
    c->buffer[c->buffer_len + 6] = (uint8_t) (total_bits >> 8);
    c->buffer[c->buffer_len + 7] = (uint8_t) (total_bits);
    c->buffer_len += 8;

    _process_block(c);

    int result_bits;
    switch (c->mode) {
        case SHA1: result_bits = 160; break;
        case SHA224:
        case SHA512_224: result_bits = 224; break;
        case SHA256:
        case SHA512_256: result_bits = 256; break;
        case SHA384: result_bits = 384; break;
        case SHA512: result_bits = 512; break;
        default:
            memset(c, 0, sizeof(*c));
            free(c);
            return;
    }

    // Digest layout (empirical): for N output words, word 0 is at digest_15to0[16 - N],
    // word N-1 at digest_15to0[15].
    int result_words = result_bits / 32;
    int base = 16 - result_words;
    for (int i = 0; i < result_words; i++) {
        uint32_t w = hw_sha->digest_15to0[base + i];
        sum[i * 4 + 0] = (uint8_t) (w >> 24);
        sum[i * 4 + 1] = (uint8_t) (w >> 16);
        sum[i * 4 + 2] = (uint8_t) (w >> 8);
        sum[i * 4 + 3] = (uint8_t) (w);
    }

    sha_destroy_context(c);
}
