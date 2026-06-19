#include "shell/commands_beken.h"
#include <stdio.h>
#include "hardware/uart.h"

#include "shell/console_colors.h"
#include "shell/Parser.h"
#include "utils/busy_wait.h"

#define UART_CLOCK_HZ    (26000000)
#define MIN_BAUDRATE     (UART_CLOCK_HZ / ((1 << 14) - 1))
#define MAX_BAUDRATE     (UART_CLOCK_HZ / 2)
#define DEFAULT_BAUDRATE (115200)

static bool valid_baudrate(int baud) { return baud > MIN_BAUDRATE && baud <= MAX_BAUDRATE; }

static int take_baudrate(const char *baud) { return take_int(baud).ok_or(0); }

int command_uart1_baudrate(int argc, const char *argv[]) {
    if (argc != 2) {
        printf(COLOR_RED("Invalid arguments") "\r\n");
        return 1;
    }

    int baudrate = take_baudrate(argv[1]);
    if (!valid_baudrate(baudrate)) {
        printf(COLOR_RED("Invalid baudrate") "%d\r\n", baudrate);
        return 2;
    }

    printf("OK %d\r\n", baudrate);
    busy_wait_ms(100);
    uart1_set_baudrate(baudrate);

    return 0;
}

int command_uart2_baudrate(int argc, const char *argv[]) {
    if (argc != 2) {
        printf(COLOR_RED("Invalid arguments") "\r\n");
        return 1;
    }

    int baudrate = take_baudrate(argv[1]);
    if (!valid_baudrate(baudrate)) {
        printf(COLOR_RED("Invalid baudrate") " %d\r\n", baudrate);
        return 2;
    }

    printf("OK %d\r\n", baudrate);
    busy_wait_ms(100);
    uart2_set_baudrate(baudrate);

    return 0;
}

int command_uart_baudrate(int argc, const char *argv[]) {
    if (argc < 2 || argc > 3) {
        printf(COLOR_RED("Invalid arguments") "\r\n");
        return 3;
    }

    if (argv[1][0] == '1' && argv[1][1] == '\0') {
        const char *arguments[2] = {argv[0], argv[2]};
        return command_uart1_baudrate(2, arguments);
    }

    if (argv[1][0] == '2' && argv[1][1] == '\0') {
        const char *arguments[2] = {argv[0], argv[2]};
        return command_uart2_baudrate(2, arguments);
    }

    printf(COLOR_RED("Invalid uart number") "\r\n");
    return 4;
}
