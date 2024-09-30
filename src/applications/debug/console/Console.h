#pragma once

#include "Packet.h"

struct Console {
    enum {
        UNKNOWN = 0x100,
        ARROW_LEFT,
        ARROW_RIGHT,
        ARROW_UP,
        ARROW_DOWN,
        HOME,
        END,
        PAGE_UP,
        PAGE_DOWN,
        DELETE,
    } KEY;

    Console();

    ~Console();

    void reset();

    void dump(const void *buf, size_t length);

    void start();

    void eol();

    void print_help();

    void update(int c);

    int resolve_key(char *in, int count);

    bool dispatch_command();

    Packet packet;
};
