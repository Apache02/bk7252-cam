#ifndef PLATFORM_BOOT_HANDLERS_H
#define PLATFORM_BOOT_HANDLERS_H

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

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_BOOT_HANDLERS_H