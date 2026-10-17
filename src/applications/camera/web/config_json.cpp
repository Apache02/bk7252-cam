#include "app/config_json.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace {

// ---- Writing ---------------------------------------------------------------

struct Writer {
    char  *buf;
    size_t size;
    size_t used = 0;
    bool   ok   = true;
    bool   first = true;

    void put(char c) {
        if (!ok) return;
        if (used + 1 >= size) {
            ok = false;
            return;
        }
        buf[used++] = c;
    }

    void text(const char *s) {
        for (; *s; s++) put(*s);
    }

    void string(const char *s) {
        put('"');
        for (; *s; s++) {
            const char c = *s;
            if (c == '"' || c == '\\') {
                put('\\');
                put(c);
            } else if (static_cast<unsigned char>(c) < 0x20) {
                char escape[7];
                snprintf(escape, sizeof(escape), "\\u%04x", static_cast<unsigned>(c) & 0x1F);
                text(escape);
            } else {
                put(c);
            }
        }
        put('"');
    }

    void key(const char *name) {
        if (!first) put(',');
        first = false;
        string(name);
        put(':');
    }

    void field(const char *name, const char *value) {
        key(name);
        string(value);
    }

    void field(const char *name, uint32_t value) {
        char digits[11];
        snprintf(digits, sizeof(digits), "%lu", static_cast<unsigned long>(value));
        key(name);
        text(digits);
    }

    void field(const char *name, bool value) {
        key(name);
        text(value ? "true" : "false");
    }
};

// ---- Reading ---------------------------------------------------------------

struct Reader {
    const char *p;
    const char *end;
};

void skip_space(Reader &r) {
    while (r.p < r.end && (*r.p == ' ' || *r.p == '\t' || *r.p == '\r' || *r.p == '\n')) r.p++;
}

bool consume(Reader &r, char c) {
    skip_space(r);
    if (r.p < r.end && *r.p == c) {
        r.p++;
        return true;
    }
    return false;
}

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Reads a string. With out == nullptr the string is only skipped. Fails on bad syntax and when the
// string does not fit in size bytes with its NUL.
bool read_string(Reader &r, char *out, size_t size) {
    if (!consume(r, '"')) return false;

    size_t len = 0;
    for (;;) {
        if (r.p >= r.end) return false;

        char c = *r.p++;
        if (c == '"') break;
        if (static_cast<unsigned char>(c) < 0x20) return false;

        if (c == '\\') {
            if (r.p >= r.end) return false;
            c = *r.p++;
            switch (c) {
                case '"':
                case '\\':
                case '/': break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                case 'u': {
                    if (r.end - r.p < 4) return false;
                    unsigned code = 0;
                    for (int i = 0; i < 4; i++) {
                        const int digit = hex_value(*r.p++);
                        if (digit < 0) return false;
                        code = code * 16 + static_cast<unsigned>(digit);
                    }
                    // Only ASCII: a string field holds bytes, not code points.
                    if (code == 0 || code >= 0x80) return false;
                    c = static_cast<char>(code);
                    break;
                }
                default: return false;
            }
        }

        if (out) {
            if (len + 1 >= size) return false;
            out[len++] = c;
        }
    }

    if (out) out[len] = '\0';
    return true;
}

bool read_number(Reader &r, uint32_t &value) {
    skip_space(r);

    uint32_t result = 0;
    int      digits = 0;
    while (r.p < r.end && *r.p >= '0' && *r.p <= '9') {
        if (++digits > 9) return false;
        result = result * 10 + static_cast<uint32_t>(*r.p++ - '0');
    }
    if (digits == 0) return false;

    value = result;
    return true;
}

bool read_word(Reader &r, const char *word) {
    skip_space(r);

    const size_t len = strlen(word);
    if (static_cast<size_t>(r.end - r.p) < len || memcmp(r.p, word, len) != 0) return false;
    r.p += len;
    return true;
}

bool read_bool(Reader &r, bool &value) {
    if (read_word(r, "true")) {
        value = true;
        return true;
    }
    if (read_word(r, "false")) {
        value = false;
        return true;
    }
    return false;
}

bool read_mac(Reader &r, uint8_t mac[6]) {
    char text[18];
    if (!read_string(r, text, sizeof(text)) || strlen(text) != 17) return false;

    for (int i = 0; i < 6; i++) {
        const int high = hex_value(text[i * 3]);
        const int low  = hex_value(text[i * 3 + 1]);
        if (high < 0 || low < 0 || (i < 5 && text[i * 3 + 2] != ':')) return false;
        mac[i] = static_cast<uint8_t>(high * 16 + low);
    }
    return true;
}

// A value of a key the config does not have: a string, a number, a bool or null.
bool skip_value(Reader &r) {
    skip_space(r);
    if (r.p < r.end && *r.p == '"') return read_string(r, nullptr, 0);

    uint32_t number;
    bool     flag;
    return read_number(r, number) || read_bool(r, flag) || read_word(r, "null");
}

bool read_field(Reader &r, const char *key, Config &config) {
    if (!strcmp(key, "mac")) return read_mac(r, config.mac);

    if (!strcmp(key, "ap_ssid")) return read_string(r, config.ap_ssid, sizeof(config.ap_ssid));
    if (!strcmp(key, "ap_channel")) {
        uint32_t channel;
        if (!read_number(r, channel) || channel > 255) return false;
        config.ap_channel = static_cast<uint8_t>(channel);
        return true;
    }
    if (!strcmp(key, "ap_enabled")) return read_bool(r, config.ap_enabled);

    if (!strcmp(key, "sta_ssid")) return read_string(r, config.sta_ssid, sizeof(config.sta_ssid));
    if (!strcmp(key, "sta_password")) return read_string(r, config.sta_password, sizeof(config.sta_password));
    if (!strcmp(key, "sta_enabled")) return read_bool(r, config.sta_enabled);

    if (!strcmp(key, "led_enabled")) return read_bool(r, config.led_enabled);
    if (!strcmp(key, "telnet_enabled")) return read_bool(r, config.telnet_enabled);

    return skip_value(r);
}

// Ranges that the radio would refuse later.
bool is_valid(const Config &config) {
    if (!config_mac_is_valid(config.mac)) return false;
    if (config.ap_ssid[0] == '\0') return false;
    if (config.ap_channel < 1 || config.ap_channel > 14) return false;

    // An empty passphrase means an open network; otherwise WPA2 wants 8 to 63 characters.
    const size_t password_len = strlen(config.sta_password);
    return password_len == 0 || password_len >= 8;
}

} // namespace

size_t config_to_json(const Config &config, char *buf, size_t size) {
    Writer w = {.buf = buf, .size = size};

    char mac[18];
    snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x", config.mac[0], config.mac[1], config.mac[2],
             config.mac[3], config.mac[4], config.mac[5]);

    w.put('{');
    w.field("mac", mac);
    w.field("ap_ssid", config.ap_ssid);
    w.field("ap_channel", static_cast<uint32_t>(config.ap_channel));
    w.field("ap_enabled", config.ap_enabled);
    w.field("sta_ssid", config.sta_ssid);
    w.field("sta_password", config.sta_password);
    w.field("sta_enabled", config.sta_enabled);
    w.field("led_enabled", config.led_enabled);
    w.field("telnet_enabled", config.telnet_enabled);
    w.put('}');

    if (!w.ok) return 0;
    buf[w.used] = '\0';
    return w.used;
}

bool config_from_json(const char *json, size_t length, Config &config) {
    Config next = config;
    Reader r    = {json, json + length};

    if (!consume(r, '{')) return false;
    if (!consume(r, '}')) {
        do {
            char key[24];
            if (!read_string(r, key, sizeof(key)) || !consume(r, ':')) return false;
            if (!read_field(r, key, next)) return false;
        } while (consume(r, ','));

        if (!consume(r, '}')) return false;
    }

    skip_space(r);
    if (r.p != r.end || !is_valid(next)) return false;

    config = next;
    return true;
}
