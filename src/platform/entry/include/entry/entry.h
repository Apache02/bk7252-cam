#ifndef PLATFORM_ENTRY_ENTRY_H
#define PLATFORM_ENTRY_ENTRY_H

#ifdef __cplusplus
extern "C" {
#endif

void do_irq(void);
void do_fiq(void);
void do_swi(void);
void do_undefined(void);
void do_pabort(void);
void do_dabort(void);
void do_reserved(void);

void vPortYieldProcessor();

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_ENTRY_ENTRY_H