#pragma once

// From a passphrase to the keys of a WPA2-PSK link (IEEE 802.11-2016, 12.7.1).

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WPA2_PMK_LEN 32
#define WPA2_PTK_LEN 48 // KCK 0..15, KEK 16..31, TK 32..47

// PBKDF2-HMAC-SHA1. False when the salt is longer than 32 bytes.
bool wpa2_pbkdf2_sha1(const uint8_t *pass, size_t pass_len, const uint8_t *salt, size_t salt_len, unsigned iterations,
                      uint8_t *out, size_t out_len);

// The PMK of a WPA2-Personal network: PBKDF2 over the passphrase with the SSID
// as salt, 4096 rounds. False, with pmk untouched, for a passphrase that is not
// 8 to 63 printable ASCII characters or an SSID that is not 1 to 32 bytes.
// Takes about half a second at 160 MHz.
bool wpa2_pmk(const char *passphrase, const uint8_t *ssid, size_t ssid_len, uint8_t pmk[WPA2_PMK_LEN]);

// The 802.11i PRF with SHA-1: HMAC(key, label || 0 || data || counter),
// concatenated until out_len bytes exist. False when label and data do not fit
// the 128-byte work buffer.
bool wpa2_prf(const uint8_t *key, size_t key_len, const char *label, const uint8_t *data, size_t data_len,
              uint8_t *out, size_t out_len);

// The pairwise transient key from the PMK, both addresses and both nonces. aa is
// the AP's address and spa ours; the order they enter in is sorted, so either
// side gets the same key.
void wpa2_ptk(const uint8_t pmk[WPA2_PMK_LEN], const uint8_t aa[6], const uint8_t spa[6], const uint8_t anonce[32],
              const uint8_t snonce[32], uint8_t ptk[WPA2_PTK_LEN]);

#ifdef __cplusplus
}
#endif
