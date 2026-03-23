#include "shell/Shell.h"
#include "shell/console_colors.h"
#include <stdio.h>
#include <string.h>


#undef count_of
#define count_of(x)     (sizeof(x) / sizeof(x[0]))

#define CONTROL_ARROW_UP        "\x1B[A"
#define CONTROL_ARROW_DOWN      "\x1B[B"
#define CONTROL_ARROW_RIGHT     "\x1B[C"
#define CONTROL_ARROW_LEFT      "\x1B[D"
#define CONTROL_HOME            "\x1B[H"
#define CONTROL_HOME_ALT        "\x1B[1~"
#define CONTROL_END             "\x1B[F"
#define CONTROL_END_ALT         "\x1B[4~"
#define CONTROL_PAGE_UP         "\x1B[5~"
#define CONTROL_PAGE_DOWN       "\x1B[6~"
#define CONTROL_DELETE          "\x1B[3~"

#define EOL                     "\r\n"


Shell::Shell(const Handler *handlers) : handlers(handlers) {
    history = new History(8);
    input = new Input();
    control_sequence.position = 0;
    control_sequence.buffer[0] = 0;
}

Shell::~Shell() {
    if (history) {
        delete history;
        history = nullptr;
    }
    delete input;
    input = nullptr;
}

void Shell::reset() {
    input->reset();
}

void Shell::start() {
    printf("%s ", ">");
}

void Shell::clear_line() {
    printf("\r\x1B[2K\r");
}

int Shell::handle_input() {
    const char *argv[32];
    int argc = 0;

    char *ptr = input->buffer;
    char *end = input->buffer + input->size;

    while (ptr < end && argc < static_cast<int>(count_of(argv) - 1)) {
        while (ptr < end && *ptr == ' ') ptr++;
        if (ptr >= end) break;

        if (*ptr == '"') {
            ptr++;
            argv[argc++] = ptr;
            while (ptr < end && *ptr != '"') ptr++;
            *ptr++ = '\0';
        } else {
            argv[argc++] = ptr;
            while (ptr < end && *ptr != ' ') ptr++;
            *ptr++ = '\0';
        }
    }
    argv[argc] = nullptr;

    if (argc < 1) return static_cast<unsigned char>(-1);

    const char *command = argv[0];

    for (int i = 0;; i++) {
        if (!handlers[i].name || !handlers[i].handler) break;

        const Handler *h = &handlers[i];
        if (strcmp(command, h->name) == 0) {
            if (history) history->add(argc, argv);
            return h->handler(argc, argv);
        }
    }

    printf(COLOR_RED("Command %s not handled\r\n"), command);
    return static_cast<unsigned char>(-2);
}

//------------------------------------------------------------------------------

void Shell::update(int c) {
    if (is_control_sequence(c)) return;

    if (c == '\t') {
        this->autocomplete();
        autocomplete_streak++;
        return;
    }
    autocomplete_streak = 0;

    if (c == '\r') c = '\n';

    if (c == '\x7F') {
        // backspace
        if (input->cursor > input->buffer) {
            printf("\b \b");
            input->remove_left();
        }

        return;
    }

    if (c == '\x03' || c == '\x04') {
        // Ctrl + C | Ctrl + D
        this->clear_line();
        this->reset();
        this->start();

        return;
    }

    if (c == '\n') {
        printf(EOL);

        if (!input->is_empty()) {
            __unused int status = handle_input();
        }

        this->reset();
        this->start();

        return;
    }

    putchar(c);
    input->put(c);

    if (input->error) {
        this->reset();
        this->start();
    }
}

int Shell::ControlSequence::detect(int c) {
    if (c == '\x1B') {
        // this is the beginning of control sequence
        position = 0;
        buffer[position++] = c;

        return IN_SEQUENCE;
    }

    if (position == 1) {
        if (c == '[') {
            buffer[position++] = c;
        } else {
            // probably incorrect
            // ignore current sequence
            position = 0;
        }

        return IN_SEQUENCE;
    }

    if (position > 1) {
        buffer[position++] = c;

        if (c >= 'A' && c <= 'Z') {
            // end of control sequence
            buffer[position] = 0;
            position = 0;
        } else if (c == '~') {
            // end of control sequence [F1-F12]
            buffer[position] = 0;
            position = 0;
        } else if (position >= count_of(buffer)) {
            // buffer overflow
            position = 0;
        }

        if (position == 0) {
            return END_SEQUENCE;
        }

        return IN_SEQUENCE;
    }

    return NO_SEQUENCE;
}

bool Shell::is_control_sequence(int c) {
    switch (control_sequence.detect(c)) {
        case ControlSequence::END_SEQUENCE:
            handle_control_sequence(control_sequence.buffer);
        case ControlSequence::IN_SEQUENCE:
            return true;
        default:
            return false;
    }
}

void Shell::handle_control_sequence(const char *control) {
    if (strcmp(control, CONTROL_ARROW_UP) == 0) {
        this->replace_command(history->prev());
    } else if (strcmp(control, CONTROL_ARROW_DOWN) == 0) {
        this->replace_command(history->next());
    } else if (strcmp(control, CONTROL_ARROW_LEFT) == 0) {
        if (input->cursor_left()) {
            printf(CONTROL_ARROW_LEFT);
        }
    } else if (strcmp(control, CONTROL_ARROW_RIGHT) == 0) {
        if (input->cursor_right()) {
            printf(CONTROL_ARROW_RIGHT);
        }
    } else if (strcmp(control, CONTROL_PAGE_UP) == 0) {
    } else if (strcmp(control, CONTROL_PAGE_DOWN) == 0) {
    } else if (strcmp(control, CONTROL_HOME) == 0 || strcmp(control, CONTROL_HOME_ALT) == 0) {
        int steps = input->get_offset();
        if (steps > 0) {
            printf("\x1B[%dD", steps);
            input->set_offset(0);
        }
    } else if (strcmp(control, CONTROL_END) == 0 || strcmp(control, CONTROL_END_ALT) == 0) {
        int steps = input->size - input->get_offset();
        if (steps > 0) {
            printf("\x1B[%dC", steps);
            input->set_offset(input->size);
        }
    } else if (strcmp(control, CONTROL_DELETE) == 0) {
        if (input->remove_right()) {
            int tail = input->size - input->get_offset();
            printf("%s \x1B[%dD", input->cursor, tail + 1);
        }
    } else {
        putchar('\r');
        printf("Unhandled control sequence [" COLOR_YELLOW("\\x%02X%s") "]", control[0], &control[1]);
        printf(EOL);
    }
}

void Shell::replace_command(const char *command) {
    this->clear_line();
    this->start();

    if (command) {
        printf("%s", command);
        input->set(command);
    } else {
        input->reset();
    }
}

static unsigned int prefix_match(const char *s1, const char *s2) {
    size_t i = 0;
    while (*s1 != '\0' && *s2 != '\0' && *s1 == *s2) {
        s1++;
        s2++;
        i++;
    }
    return i;
}

void Shell::autocomplete() {
    size_t length = input->get_offset();
    if (length == 0 || input->buffer[length - 1] == ' ') {
        return;
    }

    const char *candidates[16] = {nullptr};

    size_t found_count = 0;
    for (int i = 0;; i++) {
        if (!handlers[i].name || !handlers[i].handler) break;

        size_t match_count = prefix_match(input->buffer, handlers[i].name);
        if (match_count < length) {
            continue;
        }

        if (found_count < count_of(candidates)) {
            candidates[found_count] = handlers[i].name;
        }

        found_count++;
    }

    if (found_count == 0) return;

    if (found_count == 1) {
        const char *command = candidates[0];
        if (command[length] == '\0') {
            putchar(' ');
            input->put(' ');
        } else {
            replace_command(command);
        }
        return;
    }

    // found_count > 1
    size_t common = 0;
    for (;; common++) {
        for (size_t i = 1; i < found_count; i++) {
            if (candidates[0][common] != candidates[i][common]) goto break_2;
        }
    }
break_2:

    if (common > length) {
        for (size_t i = length; i < common; i++) {
            putchar(candidates[0][i]);
            input->put(candidates[0][i]);
        }
        autocomplete_streak = 0;
        return;
    }

    if (autocomplete_streak > 1) return;

    printf(EOL);
    for (size_t i = 0; i < found_count && i < count_of(candidates); i++) {
        if (i > 0 && (i % 4) == 0) printf(EOL);
        printf("%-16s", candidates[i]);
    }
    printf(EOL);

    this->start();
    printf("%s", input->buffer);
}

void print_command_help(const Shell::Handler *handlers) {
    printf("Commands:\r\n");
    for (int i = 0;; i++) {
        if (!handlers[i].name || !handlers[i].handler) break;

        if (handlers[i].description) {
            printf("  %-16s %s\r\n", handlers[i].name, handlers[i].description);
        } else {
            printf("  %s\r\n", handlers[i].name);
        }
    }
}
