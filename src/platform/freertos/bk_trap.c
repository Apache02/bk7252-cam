#include "utils/panic.h"


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