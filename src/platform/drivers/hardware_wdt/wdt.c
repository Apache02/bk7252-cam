#include "hardware/wdt.h"
#include "hardware/icu.h"
#include "soc/wdt.h"
#include <stdbool.h>

// Every reload writes the period too, so it has to be supplied each time. ctrl reads the
// period back, so no copy is kept.
static void wdt_reload(uint16_t period) {
    hw_write_fields(hw_wdt->ctrl,
        .period = period,
        .key = WDT_KEY_1ST,
    );
    hw_write_fields(hw_wdt->ctrl,
        .period = period,
        .key = WDT_KEY_2ND,
    );
}

void wdt_ping(void) { wdt_reload(hw_wdt->ctrl.period); }

void wdt_set(uint16_t period) { wdt_reload(period); }

void wdt_up(void) { hw_icu->peri_clk_pwd.arm_wdt = 0; }

void wdt_down(void) { hw_icu->peri_clk_pwd.arm_wdt = 1; }

int wdt_is_active(void) { return hw_icu->peri_clk_pwd.arm_wdt == 0; }

__dead2 void wdt_reboot(uint16_t delay) {
    wdt_set(delay);
    wdt_up();
    while (true);
}
