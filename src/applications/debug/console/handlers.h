#pragma once

#include "Console.h"


typedef void (ConsoleHandlerFunction)(Console &);

struct ConsoleHandler {
    const char *const name;
    const ConsoleHandlerFunction *handler;
};


extern const ConsoleHandler handlers[];
extern const size_t handlers_count;
