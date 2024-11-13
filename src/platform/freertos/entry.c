#include "hardware/intc.h"
#include "hardware/timer.h"

extern void __libc_init_array(void);

extern int main();


void entry_main(void) {
    __libc_init_array();

    intc_init();
    timer_init();

    main();
}


// eof