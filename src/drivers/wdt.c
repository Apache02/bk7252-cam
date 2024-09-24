#include "drivers/wdt.h"
#include "arch.h"


#define ICU_BASE                                     (0x00802000)
#define ICU_INTERRUPT_ENABLE                         (ICU_BASE + 16 * 4)
#define ICU_PERI_CLK_PWD                             (ICU_BASE + 2 * 4)

#define WDT_BASE                                     (0x00802900)

#define WDT_CTRL_REG                                 (WDT_BASE + 0 * 4)
#define WDT_KEY_POSI                                 (16)
#define WDT_KEY_MASK                                 (0xFF)
#define WDT_1ST_KEY                                  (0x5A)
#define WDT_2ND_KEY                                  (0xA5)

#define WDT_PERIOD_POSI                              (0)
#define WDT_PERIOD_MASK                              (0xFFFF)

#define PWD_ARM_WATCHDOG_CLK_BIT             (1 <<  8)


static unsigned int g_period;

void wdt_init(void) {
    g_period = 0;
}

void wdt_ping(void) {
    UINT32 reg;

    reg = WDT_1ST_KEY << WDT_KEY_POSI;
    reg |= (g_period & WDT_PERIOD_MASK) << WDT_PERIOD_POSI;
    REG_WRITE(WDT_CTRL_REG, reg);

    reg = WDT_2ND_KEY << WDT_KEY_POSI;
    reg |= (g_period & WDT_PERIOD_MASK) << WDT_PERIOD_POSI;
    REG_WRITE(WDT_CTRL_REG, reg);
}

void wdt_set(unsigned long period) {
    g_period = period;
    wdt_ping();
}

void wdt_up(void) {
    UINT32 reg;

    reg = REG_READ(ICU_PERI_CLK_PWD);
    reg &= ~PWD_ARM_WATCHDOG_CLK_BIT;
    REG_WRITE(ICU_PERI_CLK_PWD, reg);
}

void wdt_down(void) {
    UINT32 reg;

    reg = REG_READ(ICU_PERI_CLK_PWD);
    reg |= PWD_ARM_WATCHDOG_CLK_BIT;
    REG_WRITE(ICU_PERI_CLK_PWD, reg);
}
