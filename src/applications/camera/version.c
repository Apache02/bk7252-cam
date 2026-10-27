#include "app/version.h"

#include <stdio.h>

const char *get_version() { return BK_VERSION; }
const char *get_board_name() { return BOARD; }

void print_version() {
    printf("bk7252-cam %s by Apache02\r\n", get_version());
    printf("    board: %s\r\n\r\n", BOARD);
}
