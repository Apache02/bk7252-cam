#ifndef _PLATFORM_PORT_H
#define _PLATFORM_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

void portENABLE_IRQ(void);
void portENABLE_FIQ(void);
void portDISABLE_IRQ(void);
void portDISABLE_FIQ(void);

#ifdef __cplusplus
}
#endif

#endif // _PLATFORM_PORT_H
