#include "shell/commands_beken.h"
#include <stdio.h>
#include "hardware/random.h"

int command_random_test(__unused int argc, __unused const char *argv[]) {
    trng_enable();
    printf("0x%08lx\r\n", get_random());
    return 0;
}
