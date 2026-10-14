#include "config.h"
#include <stdio.h>
#include <string.h>
#include "hardware/random.h"
#include "platform/flash_kv.h"

// Stored keys. The numbers are persistent: never reuse or renumber one.
namespace key {
constexpr uint16_t mac            = 1;
constexpr uint16_t ap_ssid        = 2;
constexpr uint16_t ap_channel     = 3;
constexpr uint16_t sta_ssid       = 4;
constexpr uint16_t sta_password   = 5;
constexpr uint16_t sta_channel    = 6;
constexpr uint16_t led_enabled    = 7;
constexpr uint16_t telnet_enabled = 8;
} // namespace key

Config g_config = {
    .mac            = {},
    .ap_ssid        = "Beken",
    .ap_channel     = 6,
    .sta_ssid       = "",
    .sta_password   = "",
    .sta_channel    = 0,
    .led_enabled    = true,
    .telnet_enabled = true,
};

static bool mac_is_valid(const uint8_t mac[6]) {
    const uint8_t zero[6] = {};
    return !(mac[0] & 0x01) && memcmp(mac, zero, sizeof(zero)) != 0;
}

// Beken's OUI, the same one iface.c starts from.
static const uint8_t beken_oui[3] = {0xC8, 0x47, 0x8C};

// The last bit stays clear: the AP address is this one with that bit set, so no station address
// can equal the AP address of another board.
static void generate_mac(uint8_t mac[6]) {
    memcpy(mac, beken_oui, sizeof(beken_oui));
    get_random_bytes(mac + sizeof(beken_oui), 3);
    mac[5] &= 0xFE;
}

void config_load() {
    uint8_t mac[6];

    if (flash_kv_get(key::mac, mac, sizeof(mac), sizeof(mac)) == sizeof(mac) && mac_is_valid(mac)) {
        memcpy(g_config.mac, mac, sizeof(mac));
    } else {
        generate_mac(g_config.mac);
        const int err = flash_kv_set(key::mac, g_config.mac, sizeof(g_config.mac));
        if (err) printf("[config] the new mac is not stored: %d\r\n", err);
    }

    // A field that is not stored keeps its default.
    flash_kv_get_string(key::ap_ssid, g_config.ap_ssid, sizeof(g_config.ap_ssid));
    flash_kv_get_u8(key::ap_channel, &g_config.ap_channel);
    flash_kv_get_string(key::sta_ssid, g_config.sta_ssid, sizeof(g_config.sta_ssid));
    flash_kv_get_string(key::sta_password, g_config.sta_password, sizeof(g_config.sta_password));
    flash_kv_get_u8(key::sta_channel, &g_config.sta_channel);
    flash_kv_get_bool(key::telnet_enabled, &g_config.telnet_enabled);
    flash_kv_get_bool(key::led_enabled, &g_config.led_enabled);
}

int config_save() {
    int first_error = 0;

    // Every field is tried even after a failure.
    auto keep = [&first_error](int err) {
        if (err && !first_error) first_error = err;
    };

    keep(flash_kv_set(key::mac, g_config.mac, sizeof(g_config.mac)));
    keep(flash_kv_set_string(key::ap_ssid, g_config.ap_ssid));
    keep(flash_kv_set_u8(key::ap_channel, g_config.ap_channel));
    keep(flash_kv_set_string(key::sta_ssid, g_config.sta_ssid));
    keep(flash_kv_set_string(key::sta_password, g_config.sta_password));
    keep(flash_kv_set_u8(key::sta_channel, g_config.sta_channel));
    keep(flash_kv_set_bool(key::telnet_enabled, g_config.telnet_enabled));
    keep(flash_kv_set_bool(key::led_enabled, g_config.led_enabled));

    return first_error;
}

void config_ap_mac(uint8_t mac[6]) {
    memcpy(mac, g_config.mac, 6);
    mac[5] ^= 0x01;
}
