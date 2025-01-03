#include <stdio.h>
#include "hardware/wdt.h"
#include "hardware/random.h"
#include "utils/busy_wait.h"


int main() {

    trng_enable();
    printf("0x%08lx\r\n", get_random());

    busy_wait_ms(2000);
    wdt_reboot(100);

    return 0;
}
