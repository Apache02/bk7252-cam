#pragma once

#include <stdint.h>

#define CONFIG_SSID_MAX     32
#define CONFIG_PASSWORD_MAX 63

struct Config {
    uint8_t mac[6];
    // access point config
    char    ap_ssid[CONFIG_SSID_MAX + 1];
    uint8_t ap_channel;
    bool    ap_enabled;
    // station config
    char    sta_ssid[CONFIG_SSID_MAX + 1];
    char    sta_password[CONFIG_PASSWORD_MAX + 1];
    bool    sta_enabled;
    // other
    bool    led_enabled;
    bool    telnet_enabled;
};

extern Config g_config;

// Fills g_config from flash_kv. A field that is not stored keeps its default. A missing or invalid
// mac is generated and stored. The default ap_ssid is "Beken-" plus the last three mac bytes.
void config_load();

// Writes every field to flash_kv. A field that has not changed is not written again. Returns 0 or
// the first error.
int config_save();

// Puts every field back to its default and erases the stored values. The mac stays the same and is
// stored again. Returns 0 or the first error.
int config_reset();

// A station address has to be unicast and not all zeroes.
bool config_mac_is_valid(const uint8_t mac[6]);

// The AP interface cannot share the address of the station one.
void config_ap_mac(uint8_t mac[6]);
