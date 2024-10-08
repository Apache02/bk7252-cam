#include "shell/Packet.h"
#include <string.h>
#include <stdio.h>

//------------------------------------------------------------------------------

bool parse_binary_literal(const char *&cursor, int &out) {
    int accum = 0;
    int sign = 1;
    int digits = 0;

    while (*cursor && !isspace(*cursor)) {
        if (*cursor >= '0' && *cursor <= '1') {
            accum = accum * 2 + (*cursor - '0');
        } else {
            return false;
        }
        digits++;
        cursor++;
    }

    if (digits != 0) {
        out = accum;
        return true;
    } else {
        return false;
    }
}

//------------------------------------------------------------------------------

bool parse_decimal_literal(const char *&cursor, int &out) {
    int accum = 0;
    int sign = 1;
    int digits = 0;

    if (*cursor == '-') {
        sign = -1;
        cursor++;
    }

    while (*cursor && !isspace(*cursor)) {
        if (*cursor >= '0' && *cursor <= '9') {
            accum = accum * 10 + (*cursor - '0');
        } else {
            return false;
        }
        digits++;
        cursor++;
    }

    if (digits != 0) {
        out = sign * accum;
        return true;
    } else {
        return false;
    }
}

//------------------------------------------------------------------------------

bool parse_hex_literal(const char *&cursor, int &out) {
    int accum = 0;
    int sign = 1;
    int digits = 0;

    while (*cursor && !isspace(*cursor)) {
        if (*cursor >= '0' && *cursor <= '9') {
            accum = accum * 16 + (*cursor - '0');
        } else if (*cursor >= 'a' && *cursor <= 'f') {
            accum = accum * 16 + (*cursor - 'a' + 10);
        } else if (*cursor >= 'A' && *cursor <= 'F') {
            accum = accum * 16 + (*cursor - 'A' + 10);
        } else {
            return false;
        }
        digits++;
        cursor++;
    }

    if (digits != 0) {
        out = accum;
        return true;
    } else {
        return false;
    }
}

//------------------------------------------------------------------------------

bool parse_octal_literal(const char *&cursor, int &out) {
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

bool parse_int_literal(const char *&cursor, int &out) {
    auto old_cursor = cursor;
    int digit_count = 0;
    int accum = 0;
    int sign = 1;

    // Skip leading whitespace
    while (isspace(*cursor)) cursor++;

    if (*cursor != '0') {
        auto result = parse_decimal_literal(cursor, out);
        if (!result) cursor = old_cursor;
        return result;
    }
    cursor++;

    if (*cursor == 'b') {
        cursor++;
        auto result = parse_binary_literal(cursor, out);
        if (!result) cursor = old_cursor;
        return result;
    } else if (*cursor == 'x') {
        cursor++;
        auto result = parse_hex_literal(cursor, out);
        if (!result) cursor = old_cursor;
        return result;
    } else {
        auto result = parse_octal_literal(cursor, out);
        if (!result) cursor = old_cursor;
        return result;
    }
}

//------------------------------------------------------------------------------
