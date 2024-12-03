#include "hardware/intc.h"
#include "hardware/timer.h"

extern void __libc_init_array(void);

extern void __libc_fini_array(void);

extern int main();


void _start(void) {
    __libc_init_array();

    intc_init();
    timer_init();

    main();
    __libc_fini_array();
}


// eof