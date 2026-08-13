#ifndef _PLATFORM_CPU_H
#define _PLATFORM_CPU_H

// Critical section over both IRQ and FIQ - portDisableInt() sets both CPSR bits.
//
// Nesting needs no coordination: RESTORE puts back the bits DISABLE saw rather than
// enabling unconditionally, so an inner section leaves an outer one masked. The state is a
// plain local, so one DECLARATION per scope.
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
int portENABLED_IRQ(void); // 1 = CPU IRQ unmasked, 0 = masked. Read-only, no side effects.
int portENABLED_FIQ(void); // 1 = CPU FIQ unmasked, 0 = masked. Read-only, no side effects.

void WFI(void);
void arm9_enable_alignfault(void);
void arm9_disable_alignfault(void);

#ifdef __cplusplus
}
#endif

#endif // _PLATFORM_CPU_H
