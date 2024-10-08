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

private:
    History *history = nullptr;
    const Handler *handlers = nullptr;

public:
    Packet packet;

    Console(const Handler *handlers);

    ~Console();

    void reset();

    void dump(const void *buf, size_t length);

    void start();

    void eol();

    void update(int c);

    void replace_command(const char *command);

    int resolve_key(char *in, int count);

    bool dispatch_command();

    void autocomplete();
};
