#pragma once

#include "Input.h"
#include "History.h"


class Shell {
public:
    typedef int (CommandHandlerFunction)(int argc, const char *argv[]);

    struct Handler {
        const char *const name;
        CommandHandlerFunction *const handler;
        const char *const description;
    };

private:
    struct ControlSequence {
        enum {
            NO_SEQUENCE,
            IN_SEQUENCE,
            END_SEQUENCE,
        };

        char buffer[16] = {0};
        size_t position = 0;

        int detect(int c);
    } control_sequence;

    size_t autocomplete_streak = 0;

    History *history = nullptr;
    const Handler *handlers = nullptr;

    Input *input;

public:
    Shell(const Handler *handlers);

    ~Shell();

    void reset();

    void start();

    void update(int c);

    void clear_line();

    void replace_command(const char *command);

    bool is_control_sequence(int c);

    void handle_control_sequence(const char *control);

    int handle_input();

    void autocomplete();
};
