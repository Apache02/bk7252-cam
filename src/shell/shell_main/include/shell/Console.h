#pragma once

#include "Packet.h"
#include "History.h"
#include "console_colors.h"


struct Console {
public:
    typedef void (HandlerFunction)(Console &);

    struct Handler {
        const char *const name;
        HandlerFunction *const handler;
    };

private:
    char control_buf[16];
    size_t control_pos = 0;
    size_t autocomplete_streak = 0;

    History *history = nullptr;
    const Handler *handlers = nullptr;

public:
    Packet packet;

    Console(const Handler *handlers);

    ~Console();

    void reset();

    void start();

    void print_eol();

    void update(int c);

    void replace_command(const char *command);

    bool is_control_sequence(int c);

    void handle_control_sequence(const char *control);

    bool dispatch_command();

    void autocomplete();
};
