#include "hardware/wdt.h"
#include "hardware/icu.h"
#include "soc/wdt.h"
#include <stdbool.h>

static uint16_t g_period;

void wdt_init(void) { g_period = 0; }

void wdt_ping(void) {
    hw_wdt->ctrl.v = ((uint32_t)WDT_KEY_1ST << 16) | g_period;
    hw_wdt->ctrl.v = ((uint32_t)WDT_KEY_2ND << 16) | g_period;
}

void wdt_set(unsigned long period) {
    g_period = period;
    wdt_ping();
}

void wdt_up(void) { hw_icu->peri_clk_pwd.arm_wdt = 0; }

void wdt_down(void) { hw_icu->peri_clk_pwd.arm_wdt = 1; }

int wdt_is_active(void) { return hw_icu->peri_clk_pwd.arm_wdt == 0; }

void wdt_reboot(uint16_t delay) {
    wdt_set(delay);
    wdt_up();
    while (true);
}
