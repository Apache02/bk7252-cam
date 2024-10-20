#ifndef _HARDWARE_CPU_H
#define _HARDWARE_CPU_H


#ifdef __cplusplus
extern "C" {
#endif


void portENABLE_IRQ(void);
void portENABLE_FIQ(void);
int portDISABLE_IRQ(void);
int portDISABLE_FIQ(void);

int portDisableInt(void);
void portRestoreInt(int state);


#ifdef __cplusplus
}
#endif


#endif // _HARDWARE_CPU_H
