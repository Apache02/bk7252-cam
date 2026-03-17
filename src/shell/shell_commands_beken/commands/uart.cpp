#include "shell/commands_beken.h"
#include <stdio.h>
#include "hardware/uart.h"

#include "shell/console_colors.h"
#include "shell/Parser.h"


#define MIN_BAUDRATE    (26000000/((1<<14) - 1))
#define MAX_BAUDRATE    (26000000/2)

int command_uart_baudrate(int argc, const char *argv[]) {
    if (argc < 2 || argc > 3) {
        printf(COLOR_RED("Invalid arguments") "\r\n");
        return 1;
    }

    int index = take_int(argv[1]).ok_or(-1);
    int baudrate = argc == 3
                       ? take_int(argv[2]).ok_or(-1)
                       : 115200;

    if (baudrate < MIN_BAUDRATE || baudrate > MAX_BAUDRATE) {
        printf(COLOR_RED("Invalid baudrate") "%d\r\n", baudrate);
        return 3;
    }

    switch (index) {
        case 1:
            uart1_set_baudrate(baudrate);
            break;
        case 2:
            uart2_set_baudrate(baudrate);
            break;
        default:
            printf(COLOR_RED("Invalid uart number") "\r\n");
            return 2;
    }

    printf("Done\r\n");

    return 0;
}
