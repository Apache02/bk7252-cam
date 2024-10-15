#include "utils/panic.h"

extern void __libc_init_array(void);
extern int main();


void entry_main(void) {
    __libc_init_array();
    main();
}

void bk_trap_udef() {
    panic(__PRETTY_FUNCTION__);
}

void bk_trap_pabt() {
    panic(__PRETTY_FUNCTION__);
}

void bk_trap_dabt() {
    panic(__PRETTY_FUNCTION__);
}

void bk_trap_resv() {
    panic(__PRETTY_FUNCTION__);
}

void bk_trap_swi() {
    panic(__PRETTY_FUNCTION__);
}


// eof