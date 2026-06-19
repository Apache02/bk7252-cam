#ifndef _PLATFORM_CPU_H
#define _PLATFORM_CPU_H

#define GLOBAL_INT_DECLARATION() int __int_state
#define GLOBAL_INT_DISABLE()            \
    do {                                \
        __int_state = portDisableInt(); \
    } while (0)
#define GLOBAL_INT_RESTORE()         \
    do {                             \
        portRestoreInt(__int_state); \
    } while (0)

#ifdef __cplusplus
extern "C" {
#endif

int portENABLE_IRQ(void);
int portENABLE_FIQ(void);
int portDISABLE_IRQ(void);
int portDISABLE_FIQ(void);

int  portDisableInt(void);
void portRestoreInt(int state);

int portIsInIRQ(void);
int portIsInFIQ(void);

void WFI(void);
void arm9_enable_alignfault(void);
void arm9_disable_alignfault(void);

#ifdef __cplusplus
}
#endif

#endif // _PLATFORM_CPU_H
