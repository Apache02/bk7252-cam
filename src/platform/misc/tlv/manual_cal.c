#include "platform/manual_cal.h"
#include <stdio.h>
#include <string.h>

#define TXID            0x11111100u
#define TXID_ID         0x11111101u
#define TXID_MAC        0x11111102u
#define TXID_THERMAL    0x11111103u
#define TXID_CHANNEL    0x11111104u
#define TXID_XTAL       0x11111105u
#define TXID_ADC        0x11111106u
#define TXID_LPFCAP     0x11111107u

#define TXPWR_TAB_TAB            0x22222200u
#define TXPWR_ENABLE_ID          0x22222201u
#define TXPWR_TAB_B_ID           0x22222202u
#define TXPWR_TAB_G_ID           0x22222203u
#define TXPWR_TAB_N_ID           0x22222204u
#define TXPWR_TAB_DIF_GN20_ID    0x22222205u
#define TXPWR_TAB_DIF_GN40_ID    0x22222206u
#define TXPWR_TAB_BLE_ID         0x22222207u
#define TXPWR_TAB_CALI_STATUTS   0x22222208u

static uint32_t read_u32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void decode_u32_dec(const uint8_t *payload, uint32_t len, char *out, size_t out_size) {
    if (len < 4) { snprintf(out, out_size, "<short>"); return; }
    snprintf(out, out_size, "%lu", (unsigned long)read_u32le(payload));
}

static void decode_u32_hex(const uint8_t *payload, uint32_t len, char *out, size_t out_size) {
    if (len < 4) { snprintf(out, out_size, "<short>"); return; }
    snprintf(out, out_size, "0x%08lx", (unsigned long)read_u32le(payload));
}

static void decode_mac(const uint8_t *payload, uint32_t len, char *out, size_t out_size) {
    if (len < 6) { snprintf(out, out_size, "<short>"); return; }
    snprintf(out, out_size, "%02X:%02X:%02X:%02X:%02X:%02X", payload[0], payload[1], payload[2], payload[3],
             payload[4], payload[5]);
}

static void decode_lpfcap(const uint8_t *payload, uint32_t len, char *out, size_t out_size) {
    if (len < 8) { snprintf(out, out_size, "<short>"); return; }
    snprintf(out, out_size, "i=%lu q=%lu", (unsigned long)read_u32le(payload), (unsigned long)read_u32le(payload + 4));
}

static void decode_enable_flags(const uint8_t *payload, uint32_t len, char *out, size_t out_size) {
    if (len < 4) { snprintf(out, out_size, "<short>"); return; }
    uint32_t v = read_u32le(payload);
    char     names[20];
    names[0] = '\0';
    if (v & 0x1u) strcat(names, "B|");
    if (v & 0x2u) strcat(names, "G|");
    if (v & 0x4u) strcat(names, "N|");
    if (v & 0x8u) strcat(names, "BLE|");
    size_t n = strlen(names);
    if (n > 0) names[n - 1] = '\0';  // trim trailing '|'
    snprintf(out, out_size, "0x%02lx (%s)", (unsigned long)v, names[0] ? names : "none");
}

static void decode_byte_array(const uint8_t *payload, uint32_t len, char *out, size_t out_size) {
    size_t pos = 0;
    for (uint32_t i = 0; i < len; i++) {
        if (out_size - pos < 4) break;  // room for "xx " + NUL
        int n = snprintf(out + pos, out_size - pos, "%02x ", payload[i]);
        if (n <= 0) break;
        pos += (size_t)n;
    }
    if (pos > 0 && out[pos - 1] == ' ') out[pos - 1] = '\0';
}

static void decode_cali_status(const uint8_t *payload, uint32_t len, char *out, size_t out_size) {
    if (len < 4) { snprintf(out, out_size, "<short>"); return; }
    uint32_t    v    = read_u32le(payload);
    const char *name = "UNKNOWN";
    switch (v) {
        case 0: name = "NOFOUND"; break;
        case 1: name = "OK"; break;
        case 2: name = "EVM_FAIL"; break;
        case 3: name = "PWR_FAIL"; break;
    }
    snprintf(out, out_size, "%lu (%s)", (unsigned long)v, name);
}

typedef struct {
    uint32_t type;
    const char *name;
    void (*decode)(const uint8_t *payload, uint32_t len, char *out, size_t out_size);
} field_t;

static const field_t FIELDS[] = {
    {TXID, "TXID", NULL},
    {TXID_ID, "TXID_ID", decode_u32_dec},
    {TXID_MAC, "TXID_MAC", decode_mac},
    {TXID_THERMAL, "TXID_THERMAL", decode_u32_dec},
    {TXID_CHANNEL, "TXID_CHANNEL", decode_u32_dec},
    {TXID_XTAL, "TXID_XTAL", decode_u32_dec},
    {TXID_ADC, "TXID_ADC", decode_u32_hex},
    {TXID_LPFCAP, "TXID_LPFCAP", decode_lpfcap},
    {TXPWR_TAB_TAB, "TXPWR_TAB_TAB", NULL},
    {TXPWR_ENABLE_ID, "TXPWR_ENABLE_ID", decode_enable_flags},
    {TXPWR_TAB_B_ID, "TXPWR_TAB_B_ID", decode_byte_array},
    {TXPWR_TAB_G_ID, "TXPWR_TAB_G_ID", decode_byte_array},
    {TXPWR_TAB_N_ID, "TXPWR_TAB_N_ID", decode_byte_array},
    {TXPWR_TAB_DIF_GN20_ID, "TXPWR_TAB_DIF_GN20_ID", decode_u32_dec},
    {TXPWR_TAB_DIF_GN40_ID, "TXPWR_TAB_DIF_GN40_ID", decode_u32_dec},
    {TXPWR_TAB_BLE_ID, "TXPWR_TAB_BLE_ID", decode_byte_array},
    {TXPWR_TAB_CALI_STATUTS, "TXPWR_TAB_CALI_STATUTS", decode_cali_status},
};
#define FIELD_COUNT (sizeof(FIELDS) / sizeof(FIELDS[0]))

static const field_t *lookup(uint32_t type) {
    for (size_t i = 0; i < FIELD_COUNT; i++) {
        if (FIELDS[i].type == type) return &FIELDS[i];
    }
    return NULL;
}

bool tlv_manual_cal_is_container(uint32_t type) { return type == TXID || type == TXPWR_TAB_TAB; }

const char *tlv_manual_cal_name(uint32_t type) {
    const field_t *f = lookup(type);
    return f ? f->name : NULL;
}

bool tlv_manual_cal_decode(uint32_t type, const uint8_t *payload, uint32_t len, char *out, size_t out_size) {
    const field_t *f = lookup(type);
    if (!f || !f->decode) return false;

    uint32_t n = len < TLV_MANUAL_CAL_MAX_PAYLOAD ? len : TLV_MANUAL_CAL_MAX_PAYLOAD;
    f->decode(payload, n, out, out_size);
    return true;
}
