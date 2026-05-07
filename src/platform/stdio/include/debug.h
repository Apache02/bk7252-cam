#ifndef UTILS_SHELL_DEBUG_H
#define UTILS_SHELL_DEBUG_H

#include <stdint.h>
#include <stdio.h>


#ifdef DEBUG_NAME

#define _DEBUG_COLOR(color, text)     "\033[" color "m" text "\033[0m"

#define _DEBUG_COLOR_RED(text)         _DEBUG_COLOR("1;31", text)
// #define _DEBUG_COLOR_GREEN(text)       _DEBUG_COLOR("1;32", text)
#define _DEBUG_COLOR_YELLOW(text)      _DEBUG_COLOR("1;33", text)
// #define _DEBUG_COLOR_BLUE(text)        _DEBUG_COLOR("1;34", text)
// #define _DEBUG_COLOR_MAGENTA(text)     _DEBUG_COLOR("1;35", text)
#define _DEBUG_COLOR_CYAN(text)        _DEBUG_COLOR("1;36", text)
#define _DEBUG_COLOR_WHITE(text)       _DEBUG_COLOR("1;37", text)

#define LOG_PREFIX(level)    level "/" DEBUG_NAME " | "

#define LOG_D(fmt, ...)      printf(_DEBUG_COLOR_WHITE (LOG_PREFIX("D") fmt) "\r\n", ##__VA_ARGS__)
#define LOG_I(fmt, ...)      printf(_DEBUG_COLOR_CYAN  (LOG_PREFIX("I") fmt) "\r\n", ##__VA_ARGS__)
#define LOG_W(fmt, ...)      printf(_DEBUG_COLOR_YELLOW(LOG_PREFIX("W") fmt) "\r\n", ##__VA_ARGS__)
#define LOG_E(fmt, ...)      printf(_DEBUG_COLOR_RED   (LOG_PREFIX("E") fmt) "\r\n", ##__VA_ARGS__)

#define LOG_REG(x)           LOG_D("%p | [%-32s] = 0x%08lx",        \
(const void *) &(x),            \
#x,                             \
(unsigned long) (uint32_t) (x))

#else

#define LOG_D(fmt, ...)      ((void) 0)
#define LOG_I(fmt, ...)      ((void) 0)
#define LOG_W(fmt, ...)      ((void) 0)
#define LOG_E(fmt, ...)      ((void) 0)
#define LOG_REG(x)           ((void) 0)

#endif

#endif // UTILS_SHELL_DEBUG_H
