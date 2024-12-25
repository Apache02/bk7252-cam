#pragma once

#include <stdint.h>
#include <ctype.h>
#include <string.h>

bool parse_int_literal(const char *&cursor, int &out);

enum class ParseError {
    ERROR
};

template<typename R, typename E>
struct Result {

    Result(E e) {
        this->err = true;
        this->r = R();
        this->e = e;
    }

    Result(R r) {
        this->err = false;
        this->r = r;
        this->e = E();
    }

    R ok_or(R default_val) {
        return err ? default_val : r;
    }

    bool is_ok() const { return !err; }

    bool is_err() const { return err; }

    operator R() const {
        return r;
    }

    operator E() const {
        return e;
    }

    static Result Ok(R r) {
        Result result;
        result.err = false;
        result.r = r;
        result.e = E();
        return result;
    }

    static Result Err(E e) {
        Result result;
        result.err = true;
        result.r = R();
        result.e = e;
        return result;
    }

    bool err;
    R r;
    E e;
};

struct Packet {

    void clear() {
        memset(buf, 0, sizeof(buf));
        size = 0;
        error = false;
        cursor = buf;
//        packet_valid = false;
    }

    //----------------------------------------

    char to_hex(int x) {
        if (x >= 0 && x <= 9) return '0' + x;
        if (x >= 10 && x <= 15) return 'A' - 10 + x;
        return '?';
    }

    bool from_hex(char c, int &x) {
        if (c >= '0' && c <= '9') {
            x = (c - '0');
            return true;
        } else if (c >= 'a' && c <= 'f') {
            x = (10 + c - 'a');
            return true;
        } else if (c >= 'A' && c <= 'F') {
            x = (10 + c - 'A');
            return true;
        } else {
            return false;
        }
    }

    int from_hex(char c) {
        if (c >= '0' && c <= '9') {
            return (c - '0');
        } else if (c >= 'a' && c <= 'f') {
            return (10 + c - 'a');
        } else if (c >= 'A' && c <= 'F') {
            return (10 + c - 'A');
        } else {
            return -1;
        }
    }

    //----------------------------------------

    bool skip(int d) {
        if ((cursor - buf) + d > size) {
            error = true;
            return false;
        }
        cursor += d;
        return true;
    }

    //----------------------------------------

    char peek_char() {
        return (cursor - buf) >= size ? 0 : *cursor;
    }

    char take_char() {
        if ((cursor - buf) >= size) {
            error = true;
            return 0;
        } else {
            return *cursor++;
        }
    }

    void take(char c) {
        char d = take_char();
        if (c != d) {
            error = true;
        }
    }

    void take(const char *s) {
        while (*s) take(*s++);
    }

    //----------------------------------------
    // Take standard C++-format integer from packet

    Result<int, ParseError> take_int() {
        int out = 0;
        const char *temp = cursor;
        auto ok = parse_int_literal(temp, out);
        if (ok) {
            cursor = (char *) temp;
            return out;
        } else {
            return ParseError::ERROR;
        }
    }

    //----------------------------------------
    // Take fixed-length hex int from packet

    uint32_t take_hex(int digits) {
        uint32_t accum = 0;

        while (isspace(peek_char())) cursor++;

        for (int i = 0; i < digits; i++) {
            int digit = 0;
            if (!from_hex(peek_char(), digit)) {
                error = true;
                break;
            }
            accum = (accum << 4) | digit;
            cursor++;
        }

        return error ? 0 : accum;
    }

    //----------------------------------------
    // Take variable-length hex int from packet

    unsigned int take_hex() {
        int accum = 0;
        bool any_digits = false;

        while (isspace(peek_char())) cursor++;

        int digit = 0;
        for (int i = 0; i < 8; i++) {
            if (!from_hex(peek_char(), digit)) break;
            accum = (accum << 4) | digit;
            any_digits = true;
            cursor++;
        }

        if (!any_digits) error = true;
        return accum;
    }

    //----------------------------------------

    const char *take_rest_string() {
        while (isspace(peek_char())) cursor++;

        const char *ret = cursor;
        cursor = buf + size;

        return ret;
    }

    //----------------------------------------

    bool take_blob(void *blob, int size) {
        auto old_cursor = cursor;
        uint8_t *dst = (uint8_t *) blob;

        for (int i = 0; i < size; i++) {
            int lo = 0, hi = 0;
            if (((cursor - buf) <= size - 2) &&
                from_hex(cursor[0], hi) &&
                from_hex(cursor[1], lo)) {
                *dst++ = (hi << 4) | lo;
                cursor += 2;
            } else {
                error = true;
                break;
            }
        }

        if (error) cursor = old_cursor;
        return !error;
    }

    //----------------------------------------

    bool match(char c) {
        auto match = c && peek_char() == c;
        if (match) cursor++;
        return match;
    }

    //----------------------------------------

    bool match_word(const char *p) {
        auto c = cursor;
        for (; *p && *c; p++, c++) {
            if (*p != *c) return false;
        }

        bool match = *p == 0 && (isspace(*c) || *c == 0);
        if (match) cursor = c;
        return match;
    }

    bool match_prefix(const char *p) {
        auto c = cursor;
        for (; *p && *c; p++, c++) {
            if (*p != *c) return false;
        }

        bool match = *p == 0;
        if (match) cursor = c;
        return match;
    }

    bool match_prefix_hex(const char *p) {
        auto c = cursor;

        while (*p) {
            auto hi = (*p >> 4) & 0xF;
            auto lo = (*p >> 0) & 0xF;

            if (from_hex(*c++) != hi) return false;
            if (from_hex(*c++) != lo) return false;
            p++;
        }

        cursor = c;
        return true;
    }

    //----------------------------------------

    void set_packet(const char *text) {
        start_packet();
        put_str(text);
        end_packet();
    }

    void start_packet() {
        clear();
    }

    void put(char c) {
        *cursor++ = c;
        size++;
    }

    void remove_left() {
        *--cursor = 0;
        size--;
    }

    void put_str(const char *s) {
        while (*s) put(*s++);
    }

    void put_strn(const char *s, int n) {
        while (*s && n-- > 0) put(*s++);
    }

    void put_hex_u8(uint8_t x) {
        // must be high nibble first
        put(to_hex((x >> 4) & 0xF));
        put(to_hex((x >> 0) & 0xF));
    }

    void put_hex_u16_le(uint16_t x) {
        put_hex_u8(x >> 0);
        put_hex_u8(x >> 8);
    }

    void put_hex_u32_le(uint32_t x) {
        put_hex_u8(x >> 0);
        put_hex_u8(x >> 8);
        put_hex_u8(x >> 16);
        put_hex_u8(x >> 24);
    }

    void put_hex_u16_be(uint16_t x) {
        put_hex_u8(x >> 8);
        put_hex_u8(x >> 0);
    }

    void put_hex_u32_be(uint32_t x) {
        put_hex_u8(x >> 24);
        put_hex_u8(x >> 16);
        put_hex_u8(x >> 8);
        put_hex_u8(x >> 0);
    }

    void put_hex_blob(void *blob, int size) {
        uint8_t *src = (uint8_t *) blob;
        for (int i = 0; i < size; i++) {
            put_hex_u8(src[i]);
        }
    }

    void end_packet() {
//        packet_valid = true;
    }

    bool empty() {
        return cursor == buf;
    }

    //----------------------------------------

    int sentinel1 = 0xDEADBEEF;
    char buf[16384];
    int sentinel2 = 0xF00DCAFE;
    int size = 0;
    bool error = false;
    char *cursor = buf;
};
