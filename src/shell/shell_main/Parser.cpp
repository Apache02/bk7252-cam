#include "shell/Parser.h"
#include <ctype.h>
#include <stdint.h>


//------------------------------------------------------------------------------

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

//------------------------------------------------------------------------------

static bool parse_binary_literal(const char *s, int &out) {
    int accum  = 0;
    int digits = 0;

    while (*s && !isspace(static_cast<unsigned char>(*s))) {
        if (*s >= '0' && *s <= '1') {
            accum = accum * 2 + (*s - '0');
        } else {
            return false;
        }
        digits++;
        s++;
    }

    if (digits == 0) return false;

    out = accum;
    return true;
}

//------------------------------------------------------------------------------

static bool parse_decimal_literal(const char *s, int &out) {
    int accum  = 0;
    int sign   = 1;
    int digits = 0;

    if (*s == '-') {
        sign = -1;
        s++;
    }

    while (*s && !isspace(static_cast<unsigned char>(*s))) {
        if (!isdigit(static_cast<unsigned char>(*s))) return false;
        accum = accum * 10 + (*s - '0');
        digits++;
        s++;
    }

    if (digits == 0) return false;

    out = sign * accum;
    return true;
}

//------------------------------------------------------------------------------

static bool parse_hex_literal(const char *s, int &out) {
    int accum  = 0;
    int digits = 0;

    while (*s && !isspace(static_cast<unsigned char>(*s))) {
        int value = hex_nibble(*s);
        if (value < 0) {
            return false;
        }
        accum = accum * 16 + value;
        digits++;
        s++;
    }

    if (digits == 0) return false;

    out = accum;
    return true;
}

//------------------------------------------------------------------------------

static bool parse_octal_literal(const char *&cursor, int &out) {
    int accum = 0;
    int sign  = 1;

    if (*cursor == '-') {
        sign = -1;
        cursor++;
    }

    while (*cursor && !isspace(static_cast<unsigned char>(*cursor))) {
        if (*cursor >= '0' && *cursor <= '7') {
            accum = accum * 8 + (*cursor - '0');
        } else {
            return false;
        }
        cursor++;
    }

    out = sign * accum;
    return true;
}

//------------------------------------------------------------------------------

static bool parse_int_literal(const char *s, int &out) {
    // Skip leading whitespace
    while (isspace(static_cast<unsigned char>(*s))) s++;

    if (*s != '0') return parse_decimal_literal(s, out);

    s++; // skip leading 0

    if (*s == 'b') return parse_binary_literal(++s, out);
    if (*s == 'x') return parse_hex_literal(++s, out);
    return parse_octal_literal(s, out);
}

//------------------------------------------------------------------------------

Result<int, ParseError> take_int(const char *s) {
    int out = 0;
    if (parse_int_literal(s, out)) {
        return out;
    } else {
        return ParseError::ERROR;
    }
}

Result<void *, ParseError> take_pointer(const char *s) {
    int out = 0;
    if (parse_int_literal(s, out)) {
        return reinterpret_cast<void *>(out);
    } else {
        return ParseError::ERROR;
    }
}

Result<ParsedMac, ParseError> take_mac(const char *input) {
    if (!input) return ParseError::ERROR;

    const char *p = input;
    ParsedMac   out;

    // Settled after the first byte and required from then on, so one address
    // cannot mix the two spellings. Zero means the first byte was followed by
    // another digit, and no separator is accepted anywhere.
    char separator = 0;

    for (size_t i = 0; i < sizeof(out.addr); i++) {
        int h = hex_nibble(*p);
        if (h < 0) return ParseError::ERROR;
        p++;

        int l = hex_nibble(*p);
        if (l < 0) return ParseError::ERROR;
        p++;

        out.addr[i] = static_cast<uint8_t>((h << 4) | l);

        if (i + 1 == sizeof(out.addr)) break;

        if (i == 0) {
            if (*p == ':' || *p == '-') separator = *p++;
        } else if (separator) {
            if (*p != separator) return ParseError::ERROR;
            p++;
        }
    }

    if (*p != '\0') return ParseError::ERROR;

    return out;
}
