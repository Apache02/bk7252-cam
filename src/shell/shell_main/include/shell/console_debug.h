#ifndef UTILS_SHELL_DEBUG_H
#define UTILS_SHELL_DEBUG_H

#include <stdint.h>
#include <stdio.h>

#include "console_colors.h"

#ifdef DEBUG_NAME

#define LOG_PREFIX(level)    level "/" DEBUG_NAME " | "

#define LOG_D(fmt, ...)      printf(COLOR_WHITE (LOG_PREFIX("D") fmt) "\r\n", ##__VA_ARGS__)
#define LOG_I(fmt, ...)      printf(COLOR_CYAN  (LOG_PREFIX("I") fmt) "\r\n", ##__VA_ARGS__)
#define LOG_W(fmt, ...)      printf(COLOR_YELLOW(LOG_PREFIX("W") fmt) "\r\n", ##__VA_ARGS__)
#define LOG_E(fmt, ...)      printf(COLOR_RED   (LOG_PREFIX("E") fmt) "\r\n", ##__VA_ARGS__)

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
