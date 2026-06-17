// Reusable experiment sandbox — rewritten per experiment by the run-on-chip skill.
// Not a permanent test; contents change with each experiment.
#include <stdio.h>
#include "platform/stdio.h"
#include "hardware/wdt.h"
#include "hardware/flash_bypass.h"
#include "utils/busy_wait.h"

static unsigned g_passed = 0;
static unsigned g_total  = 0;

static void check(const char *label, bool ok) {
    g_total++;
    if (ok) { g_passed++; printf("[ OK ] %s\r\n", label); }
    else     {             printf("[FAIL] %s\r\n", label); }
}

int main() {
    wdt_down();
    platform_stdio_init();
    busy_wait_ms(20);
    setvbuf(stdout, NULL, _IONBF, 0);
    wdt_set(10000);
    wdt_up();

    // Test: flash_uid() returns true and at least one byte is non-zero.
    uint8_t uid[16] = {0};
    bool ok = flash_uid(uid);
    check("flash_uid returns true", ok);

    bool nonzero = false;
    for (int i = 0; i < 16; i++)
        if (uid[i]) { nonzero = true; break; }
    check("UID has at least one non-zero byte", nonzero);

    printf("uid:");
    for (int i = 0; i < 16; i++)
        printf(" %02x", (unsigned)uid[i]);
    printf("\r\n");

    printf("\r\n%u / %u passed\r\n", g_passed, g_total);
    printf("==END==\r\n");
    wdt_reboot(100);
    return 0;
}
