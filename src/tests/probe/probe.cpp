// Reusable experiment sandbox — Claude rewrites this file to test hardware
// hypotheses (registers, peripherals, driver behaviour) via the run-on-chip
// skill. Not a permanent test; contents change with each experiment.
#include <stdio.h>

#include "platform/stdio.h"
#include "hardware/wdt.h"
#include "utils/busy_wait.h"

static unsigned g_total = 0;
static unsigned g_passed = 0;

static void check(const char *name, bool ok) {
    g_total++;
    if (ok) g_passed++;
    printf("  [%s] %s\r\n", ok ? " OK " : "FAIL", name);
}

int main() {
    wdt_down();
    platform_stdio_init();
    busy_wait_ms(20);
    setvbuf(stdout, NULL, _IONBF, 0);

    wdt_set(10000);
    wdt_up();

    check("probe alive", true);

    printf("\r\n%u / %u passed\r\n", g_passed, g_total);
    printf("==END==\r\n");

    wdt_reboot(100);
    return 0;
}
