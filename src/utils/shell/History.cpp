#include "shell/History.h"
#include <string.h>


#define MIN(a, b)       (a > b ? b : a)
#define MAX(a, b)       (a < b ? b : a)

History::History(uint8_t depth) : depth(depth) {
    size = 0;
    index = 0;
    tokens = NULL;
    if (depth > 0) {
        tokens = new char *[depth];
        for (auto i = 0; i < depth; i++) {
            tokens[i] = NULL;
        }
    }
}

History::~History() {
    for (auto i = 0; i < size; i++) {
        delete tokens[i];
        tokens[i] = NULL;
    }
    delete tokens;
    tokens = NULL;
}

void History::add(const char *token) {
    index = -1;

    if (size > 0 && strcmp(token, tokens[0]) == 0) {
        return;
    }

    if (size == depth) {
        delete tokens[size - 1];
        tokens[size - 1] = NULL;
    }
    for (auto i = size; i > 0; i--) {
        if (i < depth) tokens[i] = tokens[i - 1];
    }
    tokens[0] = new char[strlen(token) + 1];
    strcpy(tokens[0], token);

    size = MIN(size + 1, depth);
}

const char *History::prev() {
    index = MIN(index + 1, size - 1);
    return tokens[index];
}

const char *History::next() {
    index = MAX(index - 1, -1);
    if (index < 0) return NULL;
    return tokens[index];
}

