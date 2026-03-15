#pragma once

#include <stdint.h>


class History {
private:
    int8_t depth;
    int8_t index;
    int8_t size;
    char **tokens;

public:
    History(int8_t depth);

    ~History();

    void add(const char *token);

    void add(int argc, const char *argv[]);

    const char *prev();

    const char *next();

    void reset_index();
};
