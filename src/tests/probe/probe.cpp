// Reusable experiment sandbox — rewritten per experiment by the run-on-chip skill.
// Not a permanent test; contents change with each experiment.
#include <stdio.h>
#include "platform/stdio.h"
#include "hardware/wdt.h"
#include "utils/busy_wait.h"

int main() {
    wdt_down();
    platform_stdio_init();
    busy_wait_ms(20);
    setvbuf(stdout, NULL, _IONBF, 0);
    wdt_set(10000);
    wdt_up();

    printf("probe alive\r\n");
    printf("==END==\r\n");
    wdt_reboot(100);
    return 0;
}
