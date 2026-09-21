#pragma once

#include <string.h>

#ifndef SHELL_INPUT_BUFFER_SIZE
#define SHELL_INPUT_BUFFER_SIZE 2048
#endif

struct Input {
    unsigned int sentinel1 = 0xDEADBEEF;
    char         buffer[SHELL_INPUT_BUFFER_SIZE]{};
    unsigned int sentinel2 = 0xF00DCAFE;
    int          size      = 0;
    bool         error     = false;
    char        *cursor    = buffer;

    void reset() {
        memset(buffer, '\0', sizeof(buffer));
        size      = 0;
        error     = false;
        cursor    = buffer;
        sentinel1 = 0xDEADBEEF;
        sentinel2 = 0xF00DCAFE;
    }

    bool check_integrity() { return sentinel1 == 0xDEADBEEF && sentinel2 == 0xF00DCAFE; }

    // Inserts at the cursor rather than appending, so the tail moves right and
    // survives. buffer[size] is kept at '\0' for whoever prints from cursor.
    void put(char c) {
        if (size >= static_cast<int>(sizeof(buffer) - 1)) {
            error = true;
            return;
        }

        int offset = get_offset();
        if (offset < size) memmove(cursor + 1, cursor, size - offset);

        *cursor++      = c;
        buffer[++size] = '\0';

        if (!check_integrity()) {
            error = true;
        }
    }

    void end() { *cursor = '\0'; }

    void set(const char *s) {
        reset();
        while (*s) put(*s++);
    }

    void put_strn(const char *s, int n) {
        while (*s && n-- > 0) put(*s++);
    }

    bool is_empty() { return buffer[0] == '\0'; }

    // ------------------------------

    // Both close the gap by pulling the tail over it, so deleting mid-line
    // shortens the text instead of cutting it off at the cursor.
    bool remove_left() {
        if (cursor <= buffer) return false;

        int offset = get_offset();
        memmove(cursor - 1, cursor, size - offset);

        cursor--;
        buffer[--size] = '\0';
        return true;
    }

    bool remove_right() {
        if (cursor >= buffer + size) return false;

        int offset = get_offset();
        memmove(cursor, cursor + 1, size - offset - 1);

        buffer[--size] = '\0';
        return true;
    }

    // ------------------------------

    bool cursor_left() {
        if (cursor > buffer) {
            cursor--;
            return true;
        }
        return false;
    }

    bool cursor_right() {
        if (cursor < buffer + size) {
            cursor++;
            return true;
        }
        return false;
    }

    // ------------------------------

    int get_offset() { return cursor - buffer; }

    void set_offset(int offset) { cursor = buffer + offset; }
};
