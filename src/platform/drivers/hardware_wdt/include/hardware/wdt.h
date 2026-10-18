#ifndef _HARDWARE_WDT_H
#define _HARDWARE_WDT_H

#include <sys/cdefs.h>
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

void wdt_ping(void);

void wdt_set(uint16_t period);

void wdt_up(void);

void wdt_down(void);

int wdt_is_active(void);

__dead2 void wdt_reboot(uint16_t delay);

#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_WDT_H
