#include "shell/Parser.h"
#include <ctype.h>
#include <stdint.h>

//------------------------------------------------------------------------------

static bool parse_binary_literal(const char *s, int &out) {
    int accum = 0;
    int digits = 0;

    while (*s && !isspace(*s)) {
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
    int accum = 0;
    int sign = 1;
    int digits = 0;

    if (*s == '-') {
        sign = -1;
        s++;
    }

    while (*s && !isspace(*s)) {
        if (!isdigit(*s)) return false;
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
    int accum = 0;
    int digits = 0;

    while (*s && !isspace(*s)) {
        if (*s >= '0' && *s <= '9') {
            accum = accum * 16 + (*s - '0');
        } else if (*s >= 'a' && *s <= 'f') {
            accum = accum * 16 + (*s - 'a' + 10);
        } else if (*s >= 'A' && *s <= 'F') {
            accum = accum * 16 + (*s - 'A' + 10);
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

static bool parse_octal_literal(const char *&cursor, int &out) {
    int accum = 0;
    int sign = 1;

    if (*cursor == '-') {
        sign = -1;
        cursor++;
    }

    while (*cursor && !isspace(*cursor)) {
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
    while (isspace(*s)) s++;

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

