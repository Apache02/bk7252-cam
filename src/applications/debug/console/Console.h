#pragma once

#include "Packet.h"

struct Console {
    Console();

    ~Console();

    void reset();

    void dump(const void *buf, size_t length);

    void start();

    void print_help();

    void update(char *input, unsigned int count);

    bool dispatch_command();

    Packet packet;
};
