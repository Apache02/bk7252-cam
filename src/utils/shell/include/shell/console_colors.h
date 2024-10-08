#ifndef UTILS_SHELL_COLORS_H
#define UTILS_SHELL_COLORS_H

#define CONSOLE_COLOR(color, text)     "\u001b[" color "m" text "\u001b[0m"

#define COLOR_RED(text)         CONSOLE_COLOR("1;31", text)
#define COLOR_GREEN(text)       CONSOLE_COLOR("1;32", text)
#define COLOR_YELLOW(text)      CONSOLE_COLOR("1;33", text)
#define COLOR_BLUE(text)        CONSOLE_COLOR("1;34", text)
#define COLOR_MAGENTA(text)     CONSOLE_COLOR("1;35", text)
#define COLOR_CYAN(text)        CONSOLE_COLOR("1;36", text)
#define COLOR_WHITE(text)       CONSOLE_COLOR("1;37", text)

#endif // UTILS_SHELL_COLORS_H