#include "utils/panic.h"


void bk_trap_udef() {
    panic_blink(1);
    panic("bk_trap_udef");
}

void bk_trap_pabt() {
    panic_blink(2);
    panic("bk_trap_pabt");
}

void bk_trap_dabt() {
    panic_blink(3);
    panic("bk_trap_dabt");
}

void bk_trap_resv() {
    panic_blink(4);
    panic("bk_trap_resv");
}

void bk_trap_swi() {
    panic_blink(5);
    panic("bk_trap_swi");
}


// eof