#include "shell/commands_beken.h"
#include <stdio.h>
#include "hardware/random.h"


void command_random_test(__unused Console &c) {
    trng_enable();
    printf("0x%08lx\r\n", get_random());
}
