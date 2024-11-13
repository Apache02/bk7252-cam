#include "utils/panic.h"


void bk_trap_udef() {
    panic_blink(1);
    panic(__PRETTY_FUNCTION__);
}

void bk_trap_pabt() {
    panic_blink(2);
    panic(__PRETTY_FUNCTION__);
}

void bk_trap_dabt() {
    panic_blink(3);
    panic(__PRETTY_FUNCTION__);
}

void bk_trap_resv() {
    panic_blink(4);
    panic(__PRETTY_FUNCTION__);
}

void bk_trap_swi() {
    panic_blink(5);
    panic(__PRETTY_FUNCTION__);
}


// eof