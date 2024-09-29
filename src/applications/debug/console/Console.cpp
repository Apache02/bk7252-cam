#include "Console.h"
#include "colors.h"

#include <stdio.h>
#include <functional>

#include "commands/commands.h"


//------------------------------------------------------------------------------

struct ConsoleHandler {
    const char *name;
    const std::function<void(Console &)> handler;
};

static const ConsoleHandler handlers[] = {
        {
                "help",
                [](Console &c) {
                    c.print_help();
                },
        },
        {
                "echo",
                command_echo,
        },
        {
                "reboot",
                command_reboot,
        },
        {
                "dump",
                command_dump,
        },
        {
                "dump32",
                command_dump32,
        },
};

static const int handler_count = sizeof(handlers) / sizeof(handlers[0]);

//------------------------------------------------------------------------------

Console::Console() {}

Console::~Console() {}

void Console::reset() {}

void Console::dump(const void *buf, size_t length) {
    const uint8_t *ptr = static_cast<const uint8_t *>(buf);
    size_t i;

    for (i = 0; i < length; i++) {
        printf("%02X ", ptr[i]);

        if ((i + 1) % 16 == 0) {
            printf("\r\n");
        }
    }

    if (i % 16 != 0) {
        printf("\r\n");
    }
}

void Console::start() {
    printf("%s ", ">");
}

void Console::print_help() {
    printf("Commands:\r\n");
    for (int i = 0; i < handler_count; i++) {
        printf("  %s\r\n", handlers[i].name);
    }
}

bool Console::dispatch_command() {
    for (int i = 0; i < handler_count; i++) {
        const ConsoleHandler *h = &handlers[i];
        if (packet.match_word(h->name)) {
            h->handler(*this);
            return true;
        }
    }
    printf(COLOR_RED("Command %s not handled\r\n"), packet.buf);
    return false;
}

//------------------------------------------------------------------------------

void Console::update(char *input, unsigned int count) {
    if (!count) return;

    if (input[0] == '\t') {
        input[0] = ' ';
    }

    if (input[0] == 0x7F /*backspace*/) {
        if (packet.cursor2 > packet.buf) {
            printf("\b \b");
            packet.cursor2--;
            packet.size--;
            *packet.cursor2 = 0;
        }
    } else if (input[0] == '\n' || input[0] == '\r') {
        packet.buf[packet.size] = 0;
        packet.cursor2 = packet.buf;

        putchar('\r');
        putchar('\n');

        if (packet.buf[0] != '\0') {
//            absolute_time_t time_before = get_absolute_time();
            bool is_handled = dispatch_command();
//            absolute_time_t time_after = get_absolute_time();

            if (is_handled) {
//                printf("Command took %lld us\n", absolute_time_diff_us(time_before, time_after));
                if ((packet.cursor2 - packet.buf) < packet.size) {
                    printf(COLOR_RED("Leftover text in packet - {%s}\r\n"), packet.cursor2);
                }
            }
        }

        packet.clear();
        this->start();
    } else if (count > 1) {
        bool process_putchar = false;
        if (count == 3 && input[0] == '\x1B' && input[1] == '\x5B') {
            switch (input[2]) {
                case '\x41':        // arrow up
                case '\x42':        // arrow down
                    break;
                case '\x43':        // arrow right
                    if (*packet.cursor2 != '\0') {
                        packet.cursor2++;
                        process_putchar = true;
                    }
                    break;
                case '\x44':        // arrow left
                    if (packet.cursor2 > packet.buf) {
                        packet.cursor2--;
                        process_putchar = true;
                    }
                    break;
                default:
                    // unknown key
                    break;
            }
        }

        if (process_putchar) {
            for (auto i = 0; i < count; i++) {
                putchar(input[i]);
            }
        }
    } else {
        putchar(input[0]);
        packet.put(input[0]);
    }
}

//------------------------------------------------------------------------------
