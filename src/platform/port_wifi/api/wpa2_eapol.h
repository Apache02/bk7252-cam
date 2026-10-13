#pragma once

// EAPOL-Key frames (IEEE 802.11-2016, 12.7.2) as a WPA2-PSK station sees them.
// Frames here start at the 802.1X header: version, packet type, a two-byte body
// length, then the key descriptor. The Ethernet header is the caller's.
//
// Pure C, no RTOS, so the host tests compile it as it is.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Key information bits.
#define KEY_INFO_VERSION_MASK 0x0007 // 2 = HMAC-SHA1-128 MIC and AES key wrap
#define KEY_INFO_PAIRWISE     0x0008
#define KEY_INFO_INSTALL      0x0040
#define KEY_INFO_ACK          0x0080
#define KEY_INFO_MIC          0x0100
#define KEY_INFO_SECURE       0x0200
#define KEY_INFO_ENCRYPTED    0x1000

// The key information of what this station sends, with the version bits set to 2.
#define KEY_INFO_MSG2       0x010A // pairwise, MIC
#define KEY_INFO_MSG4       0x030A // pairwise, MIC, secure
#define KEY_INFO_GROUP_MSG2 0x0302 // MIC, secure

#define WPA2_EAPOL_HEADER_LEN 4
#define WPA2_EAPOL_KEY_FIXED  99 // header and descriptor, up to the key data
#define WPA2_EAPOL_MIC_OFFSET 81
#define WPA2_EAPOL_MIC_LEN    16

// The RSN information element this station offers: CCMP group, CCMP pairwise,
// PSK, no capabilities. The association request and message 2 carry these bytes.
extern const uint8_t WPA2_RSN_IE[22];

// A parsed frame. The pointers point into the frame that was parsed.
struct wpa2_eapol_key {
    uint16_t       key_info;
    uint16_t       key_length;
    uint8_t        replay[8];
    const uint8_t *nonce; // 32 bytes
    const uint8_t *key_data;
    uint16_t       key_data_len;
    size_t         frame_len; // the 802.1X header and its body; padding after is not counted
};

// False for anything that is not a well-formed EAPOL-Key frame of the RSN
// descriptor type: too short, a body or key data length that runs past the end.
bool wpa2_eapol_parse(const uint8_t *frame, size_t len, struct wpa2_eapol_key *out);

// Builds a frame into out and returns its length, or 0 when it does not fit. The
// IV, RSC and key identifier are zero. With a kck the MIC is computed and
// written; without, it stays zero.
size_t wpa2_eapol_build(uint8_t *out, size_t out_size, uint16_t key_info, uint16_t key_length,
                        const uint8_t replay[8], const uint8_t nonce[32], const uint8_t *key_data,
                        uint16_t key_data_len, const uint8_t kck[16]);

// Checks the MIC of a frame (frame_len from the parse) with the MIC field read as zero.
bool wpa2_eapol_mic_ok(const uint8_t kck[16], const uint8_t *frame, size_t frame_len);

// Finds the GTK KDE (vendor element 00-0F-AC type 1) in decrypted key data.
bool wpa2_find_gtk(const uint8_t *key_data, size_t len, uint8_t *key_id, uint8_t gtk[16]);

#ifdef __cplusplus
}
#endif
