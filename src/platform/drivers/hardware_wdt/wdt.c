#include "hardware/wdt.h"
#include "hardware/icu.h"


#define WDT_BASE                (0x00802900)
#define WDT_CTRL_REG            (WDT_BASE + 0 * 4)

#define hw_wdt                  ((volatile uint32_t *) WDT_CTRL_REG)


static uint16_t g_period;

void wdt_init(void) {
    g_period = 0;
}

void wdt_ping(void) {
    *hw_wdt = (g_period & 0xFFFF) | 0x5A0000;
    *hw_wdt = (g_period & 0xFFFF) | 0xA50000;
}

void wdt_set(unsigned long period) {
    g_period = period;
    wdt_ping();
}

void wdt_up(void) {
    hw_icu->peri_clk_pwd.arm_wdt = 0;
}

void wdt_down(void) {
    hw_icu->peri_clk_pwd.arm_wdt = 1;
}

int wdt_is_active(void) {
    return hw_icu->peri_clk_pwd.arm_wdt == 0;
}

void wdt_reboot(uint16_t delay) {
    wdt_set(delay);
    wdt_up();
    while (true);
}
