#include "Console.h"
#include "utils/console_colors.h"
#include "handlers.h"

#include <stdio.h>


Console::Console() {}

Console::~Console() {}

void Console::reset() {}

__unused void Console::dump(const void *buf, size_t length) {
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

void Console::eol() {
    printf("\r\n");
}

bool Console::dispatch_command() {
    for (int i = 0; i < handlers_count; i++) {
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

void Console::update(int c) {
    if (c == '\t') {
        // TODO: autocomplete
        c = ' ';
    }

    if (c == '\r') c = '\n';

    if (c == '\x7F') {
        // backspace
        if (packet.cursor2 > packet.buf) {
            printf("\b \b");
            packet.cursor2--;
            packet.size--;
            *packet.cursor2 = 0;
        }

        return;
    }

    if (c == '\x03' || c == '\x04') {
        // Ctrl + C | Ctrl + D
        this->eol();

        packet.clear();
        this->start();

        return;
    }

    if (c == '\n') {
        packet.buf[packet.size] = 0;
        packet.cursor2 = packet.buf;

        this->eol();

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

        return;
    }

    if (c > 0xFF) {
        switch (c) {
            case ARROW_RIGHT:
                if (*packet.cursor2 != '\0') {
                    packet.cursor2++;
                    printf("\x1B\x5B\x43");
                }
                break;
            case ARROW_LEFT:
                if (packet.cursor2 > packet.buf) {
                    packet.cursor2--;
                    printf("\x1B\x5B\x44");
                }
                break;
        }

        return;
    }

    putchar(c);
    packet.put(c);
}

int Console::resolve_key(char *in, __unused int count) {
    if (in[0] == '\x1B') {
        if (in[1] == '\x5B' && in[2] == '\x41') {
            return Console::ARROW_UP;
        } else if (in[1] == '\x5B' && in[2] == '\x42') {
            return Console::ARROW_DOWN;
        } else if (in[1] == '\x5B' && in[2] == '\x43') {
            return Console::ARROW_RIGHT;
        } else if (in[1] == '\x5B' && in[2] == '\x44') {
            return Console::ARROW_LEFT;
        } else if (in[1] == '\x5B' && in[2] == '\x46') {
            return Console::END;
        } else if (in[1] == '\x5B' && in[2] == '\x48') {
            return Console::HOME;
        } else if (in[1] == '\x5B' && in[2] == '\x33' && in[3] == '\x7E') {
            return Console::DELETE;
        } else if (in[1] == '\x5B' && in[2] == '\x35' && in[3] == '\x7E') {
            return Console::PAGE_UP;
        } else if (in[1] == '\x5B' && in[2] == '\x36' && in[3] == '\x7E') {
            return Console::PAGE_DOWN;
        }
    }

    return UNKNOWN;
}


//------------------------------------------------------------------------------
