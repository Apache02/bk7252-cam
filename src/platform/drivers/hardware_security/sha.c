#include "regs.h"
#include "hardware/sha.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


#define SHA_BLOCK_SIZE_SMALL  64   // SHA-1/224/256
#define SHA_BLOCK_SIZE_LARGE  128  // SHA-384/512 family


typedef enum {
    SHA1 = 0,
    SHA224 = 2,
    SHA256 = 3,
    SHA512_224 = 4,
    SHA512_256 = 5,
    SHA384 = 6,
    SHA512 = 7
} SHA_MODE;

// Per-mode sizes in bits, indexed by SHA_MODE (slots 0..7).
// state_bits drives register layout (block + digest base); digest_bits is the output length.
static const uint16_t sha_state_bits[8] = {
    [SHA1] = 160,
    [SHA224] = 256,
    [SHA256] = 256,
    [SHA384] = 512,
    [SHA512_224] = 512,
    [SHA512_256] = 512,
    [SHA512] = 512,
};
static const uint16_t sha_digest_bits[8] = {
    [SHA1] = 160,
    [SHA224] = 224,
    [SHA256] = 256,
    [SHA384] = 384,
    [SHA512] = 512,
    [SHA512_224] = 224,
    [SHA512_256] = 256,
};

typedef struct {
    SHA_MODE mode;
    bool first_block;
    uint32_t block_size;         // 64 for SHA-1/224/256, 128 for SHA-384/512
    uint64_t total;              // bytes accumulated so far
    uint32_t buffer_len;         // bytes currently in buffer
    unsigned char buffer[SHA_BLOCK_SIZE_LARGE];
} sha_context_t;


static inline uint32_t load_be32(const uint8_t *p) {
    return ((uint32_t) p[0] << 24)
           | ((uint32_t) p[1] << 16)
           | ((uint32_t) p[2] << 8)
           | ((uint32_t) p[3]);
}

static inline void store_be32(uint8_t *p, uint32_t w) {
    p[0] = (uint8_t) (w >> 24);
    p[1] = (uint8_t) (w >> 16);
    p[2] = (uint8_t) (w >> 8);
    p[3] = (uint8_t) w;
}

static inline void wait_ready(void) {
    while (!hw_sha->status.ready) ;
}

static void process_block(sha_context_t *c) {
    // Block layout (empirical, BK7252):
    //   message word 0 sits at the tail of the register file at block_31to0[32 - block_words].
    //   SHA-1/224/256 (16 words) use block_31to0[16..31]; SHA-384/512 (32 words) use [0..31].
    int block_words = (int) (c->block_size / sizeof(uint32_t));
    int base = 32 - block_words;

    for (int i = 0; i < block_words; i++) {
        hw_sha->block_31to0[base + i] = load_be32(c->buffer + i * 4);
    }

    // INIT (bit 0) for the first block: resets state to H0 and processes.
    // NEXT (bit 1) for subsequent blocks: continues from current state.
    if (c->first_block) {
        hw_sha->control.init = 1;
        c->first_block = false;
    } else {
        hw_sha->control.next = 1;
    }
    wait_ready();
}

void *sha_create_context(SHA_MODE mode) {
    sha_context_t *ctx = (sha_context_t *) malloc(sizeof(sha_context_t));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(sha_context_t));
    ctx->mode = mode;
    ctx->first_block = true;
    ctx->block_size = (mode <= SHA256) ? SHA_BLOCK_SIZE_SMALL : SHA_BLOCK_SIZE_LARGE;

    hw_sha->config.enable = 1;
    hw_sha->config.mode = mode;

    return ctx;
}

void *sha1_create_context() { return sha_create_context(SHA1); }
void *sha224_create_context() { return sha_create_context(SHA224); }
void *sha256_create_context() { return sha_create_context(SHA256); }
void *sha512_create_context() { return sha_create_context(SHA512); }

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
            process_block(c);
            c->buffer_len = 0;
        }
    }
}

void sha_finish(void *ctx, uint8_t *sum) {
    sha_context_t *c = (sha_context_t *) ctx;
    uint64_t total_bits = c->total * 8;

    // Length field: 8 bytes for SHA-1/224/256, 16 bytes for SHA-384/512.
    size_t length_size = (c->block_size == SHA_BLOCK_SIZE_LARGE) ? 16 : 8;
    size_t padding_end = c->block_size - length_size;

    c->buffer[c->buffer_len++] = 0x80;

    // If the length field doesn't fit in this block, flush a zero-padded block first.
    if (c->buffer_len > padding_end) {
        memset(c->buffer + c->buffer_len, 0, c->block_size - c->buffer_len);
        process_block(c);
        c->buffer_len = 0;
    }
    memset(c->buffer + c->buffer_len, 0, padding_end - c->buffer_len);

    // SHA-384/512 length is 128-bit; high 8 bytes are zero since total fits in 64 bits.
    uint8_t *len_tail = c->buffer + c->block_size - 8;
    for (int i = 0; i < 8; i++) {
        len_tail[i] = (uint8_t) (total_bits >> ((7 - i) * 8));
    }
    process_block(c);

    // Digest layout (empirical): the hardware state sits in the last (state_bits/32) slots
    // of digest_15to0; truncated variants (SHA-224, SHA-384, SHA-512/t) read the first
    // (digest_bits/32) words from that region.
    int base = 16 - sha_state_bits[c->mode] / 32;
    int result_words = sha_digest_bits[c->mode] / 32;
    for (int i = 0; i < result_words; i++) {
        store_be32(sum + i * 4, hw_sha->digest_15to0[base + i]);
    }

    sha_destroy_context(c);
}
