#ifndef UTILS_SHELL_DEBUG_H
#define UTILS_SHELL_DEBUG_H

#include "console_colors.h"

#ifdef DEBUG_NAME

#define LOG_D(fmt, ...)      printf(COLOR_WHITE(    "D/" DEBUG_NAME " | " fmt) "\r\n", ##__VA_ARGS__)
#define LOG_I(fmt, ...)      printf(COLOR_CYAN(     "I/" DEBUG_NAME " | " fmt) "\r\n", ##__VA_ARGS__)
#define LOG_W(fmt, ...)      printf(COLOR_YELLOW(   "W/" DEBUG_NAME " | " fmt) "\r\n", ##__VA_ARGS__)
#define LOG_E(fmt, ...)      printf(COLOR_RED(      "E/" DEBUG_NAME " | " fmt) "\r\n", ##__VA_ARGS__)

#else

#define LOG_I(fmt, ...)
#define LOG_D(fmt, ...)
#define LOG_W(fmt, ...)
#define LOG_E(fmt, ...)

#endif

#endif // UTILS_SHELL_DEBUG_H