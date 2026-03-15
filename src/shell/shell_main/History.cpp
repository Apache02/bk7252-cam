#include "shell/History.h"

#include <string.h>


History::History(int8_t depth) : depth(depth < 0 ? 0 : depth), index(-1), size(0), tokens(nullptr) {
    if (depth > 0) {
        tokens = new char *[depth];
        for (uint8_t i = 0; i < depth; i++) {
            tokens[i] = nullptr;
        }
    }
}

History::~History() {
    for (uint8_t i = 0; i < size; i++) {
        delete[] tokens[i];
        tokens[i] = nullptr;
    }
    delete[] tokens;
    tokens = nullptr;
}

void History::add(const char *token) {
    if (!token || token[0] == '\0') return;

    index = -1;

    if (size > 0 && strcmp(token, tokens[0]) == 0) {
        return;
    }

    if (size == depth) {
        char *evicted = tokens[size - 1];
        for (uint8_t i = size - 1; i > 0; i--) {
            tokens[i] = tokens[i - 1];
        }
        delete[] evicted;
    } else {
        for (uint8_t i = size; i > 0; i--) {
            tokens[i] = tokens[i - 1];
        }
        size++;
    }

    tokens[0] = new char[strlen(token) + 1];
    strcpy(tokens[0], token);
}

void History::add(int argc, const char *argv[]) {
    if (argc <= 0) return;

    size_t total_len = 0;
    for (int i = 0; i < argc; i++) {
        total_len += strlen(argv[i]);
    }
    total_len += (argc - 1); // spaces

    char *token = new char[total_len + 1];

    char *ptr = token;
    for (int i = 0; i < argc; i++) {
        if (i > 0) *ptr++ = ' ';
        const char *src = argv[i];
        while (*src) *ptr++ = *src++;
    }
    *ptr = '\0';

    add(token);
    delete[] token;
}

const char *History::prev() {
    if (size == 0) return nullptr;
    if (index + 1 < (int8_t) size) index++;
    return tokens[index];
}

const char *History::next() {
    if (index <= 0) {
        index = -1;
        return nullptr;
    }
    index--;
    return tokens[index];
}

void History::reset_index() {
    index = -1;
}
