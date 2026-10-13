#include "wpa2_aes.h"

#include <string.h>

static uint8_t s_sbox[256];
static uint8_t s_inv[256];
static bool    s_ready;

static uint8_t rotl8(uint8_t x, int n) { return (uint8_t)((x << n) | (x >> (8 - n))); }
static uint8_t xtime(uint8_t x) { return (uint8_t)((x << 1) ^ ((x >> 7) * 0x1b)); }

// The S-box is the multiplicative inverse in GF(2^8) followed by an affine map.
// Walking p through the powers of 3 and q through their inverses visits every
// non-zero element once, so no table has to be typed in.
static void init_tables(void) {
    uint8_t p = 1, q = 1;
    do {
        p = (uint8_t)(p ^ (p << 1) ^ ((p & 0x80) ? 0x1b : 0));
        q ^= (uint8_t)(q << 1);
        q ^= (uint8_t)(q << 2);
        q ^= (uint8_t)(q << 4);
        if (q & 0x80) q ^= 0x09;
        s_sbox[p] = (uint8_t)(q ^ rotl8(q, 1) ^ rotl8(q, 2) ^ rotl8(q, 3) ^ rotl8(q, 4) ^ 0x63);
    } while (p != 1);
    s_sbox[0] = 0x63;
    for (int i = 0; i < 256; i++) s_inv[s_sbox[i]] = (uint8_t)i;
    s_ready = true;
}

static void expand(const uint8_t key[16], uint8_t rk[176]) {
    memcpy(rk, key, 16);
    uint8_t rcon = 1;
    for (int i = 16; i < 176; i += 4) {
        uint8_t t[4];
        memcpy(t, rk + i - 4, 4);
        if (i % 16 == 0) {
            uint8_t u = t[0];
            t[0]      = (uint8_t)(s_sbox[t[1]] ^ rcon);
            t[1]      = s_sbox[t[2]];
            t[2]      = s_sbox[t[3]];
            t[3]      = s_sbox[u];
            rcon      = xtime(rcon);
        }
        for (int j = 0; j < 4; j++) rk[i + j] = (uint8_t)(rk[i + j - 16] ^ t[j]);
    }
}

static void add_key(uint8_t s[16], const uint8_t *k) {
    for (int i = 0; i < 16; i++) s[i] ^= k[i];
}

static void sub_bytes(uint8_t s[16], const uint8_t *box) {
    for (int i = 0; i < 16; i++) s[i] = box[s[i]];
}

// The state is column-major: byte 4c+r is row r of column c.
static void shift_rows(uint8_t s[16]) {
    uint8_t t[16];
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) t[4 * c + r] = s[4 * ((c + r) % 4) + r];
    }
    memcpy(s, t, 16);
}

static void inv_shift_rows(uint8_t s[16]) {
    uint8_t t[16];
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) t[4 * ((c + r) % 4) + r] = s[4 * c + r];
    }
    memcpy(s, t, 16);
}

static void mix_columns(uint8_t s[16]) {
    for (int c = 0; c < 4; c++) {
        uint8_t *a  = s + 4 * c;
        uint8_t  t  = (uint8_t)(a[0] ^ a[1] ^ a[2] ^ a[3]);
        uint8_t  a0 = a[0];
        a[0] ^= (uint8_t)(t ^ xtime((uint8_t)(a[0] ^ a[1])));
        a[1] ^= (uint8_t)(t ^ xtime((uint8_t)(a[1] ^ a[2])));
        a[2] ^= (uint8_t)(t ^ xtime((uint8_t)(a[2] ^ a[3])));
        a[3] ^= (uint8_t)(t ^ xtime((uint8_t)(a[3] ^ a0)));
    }
}

static void inv_mix_columns(uint8_t s[16]) {
    for (int c = 0; c < 4; c++) {
        uint8_t *a = s + 4 * c;
        uint8_t  u = xtime(xtime((uint8_t)(a[0] ^ a[2])));
        uint8_t  v = xtime(xtime((uint8_t)(a[1] ^ a[3])));
        a[0] ^= u;
        a[1] ^= v;
        a[2] ^= u;
        a[3] ^= v;
    }
    mix_columns(s);
}

void wpa2_aes128_encrypt(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]) {
    if (!s_ready) init_tables();

    uint8_t rk[176], s[16];
    expand(key, rk);
    memcpy(s, in, 16);

    add_key(s, rk);
    for (int r = 1; r < 10; r++) {
        sub_bytes(s, s_sbox);
        shift_rows(s);
        mix_columns(s);
        add_key(s, rk + 16 * r);
    }
    sub_bytes(s, s_sbox);
    shift_rows(s);
    add_key(s, rk + 160);

    memcpy(out, s, 16);
}

void wpa2_aes128_decrypt(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]) {
    if (!s_ready) init_tables();

    uint8_t rk[176], s[16];
    expand(key, rk);
    memcpy(s, in, 16);

    add_key(s, rk + 160);
    for (int r = 9; r >= 1; r--) {
        inv_shift_rows(s);
        sub_bytes(s, s_inv);
        add_key(s, rk + 16 * r);
        inv_mix_columns(s);
    }
    inv_shift_rows(s);
    sub_bytes(s, s_inv);
    add_key(s, rk);

    memcpy(out, s, 16);
}

static const uint8_t k_iv[8] = {0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6};

void wpa2_aes_key_wrap(const uint8_t kek[16], const uint8_t *plain, size_t plain_len, uint8_t *out) {
    size_t  n = plain_len / 8;
    uint8_t a[8];
    memcpy(a, k_iv, 8);
    memcpy(out + 8, plain, plain_len);

    for (size_t j = 0; j < 6; j++) {
        for (size_t i = 1; i <= n; i++) {
            uint8_t b[16];
            memcpy(b, a, 8);
            memcpy(b + 8, out + 8 * i, 8);
            wpa2_aes128_encrypt(kek, b, b);

            uint32_t t = (uint32_t)(n * j + i);
            memcpy(a, b, 8);
            a[7] ^= (uint8_t)t;
            a[6] ^= (uint8_t)(t >> 8);
            memcpy(out + 8 * i, b + 8, 8);
        }
    }
    memcpy(out, a, 8);
}

bool wpa2_aes_key_unwrap(const uint8_t kek[16], const uint8_t *in, size_t in_len, uint8_t *out) {
    if (in_len < 24 || in_len % 8) return false;

    size_t  n = in_len / 8 - 1;
    uint8_t a[8];
    memcpy(a, in, 8);
    memcpy(out, in + 8, in_len - 8);

    for (int j = 5; j >= 0; j--) {
        for (size_t i = n; i >= 1; i--) {
            uint32_t t = (uint32_t)(n * (size_t)j + i);
            uint8_t  b[16];
            memcpy(b, a, 8);
            b[7] ^= (uint8_t)t;
            b[6] ^= (uint8_t)(t >> 8);
            memcpy(b + 8, out + 8 * (i - 1), 8);
            wpa2_aes128_decrypt(kek, b, b);

            memcpy(a, b, 8);
            memcpy(out + 8 * (i - 1), b + 8, 8);
        }
    }
    return memcmp(a, k_iv, 8) == 0;
}
