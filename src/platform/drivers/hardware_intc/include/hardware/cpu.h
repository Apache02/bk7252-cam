#ifndef _HARDWARE_CPU_H
#define _HARDWARE_CPU_H


#define GLOBAL_INT_DECLARATION()        int __int_state
#define GLOBAL_INT_DISABLE()            do { __int_state = portDisableInt(); } while(0)
#define GLOBAL_INT_RESTORE()            do { portRestoreInt(__int_state); } while (0)


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
