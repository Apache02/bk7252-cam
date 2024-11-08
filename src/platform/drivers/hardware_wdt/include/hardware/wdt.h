#ifndef _HARDWARE_WDT_H
#define _HARDWARE_WDT_H

#include "stdint.h"


#ifdef __cplusplus
extern "C" {
#endif

void wdt_init(void);

void wdt_ping(void);

void wdt_set(unsigned long period);

void wdt_up(void);

void wdt_down(void);

int wdt_is_active(void);

void wdt_reboot(uint16_t delay);


#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_WDT_H
