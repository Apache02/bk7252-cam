#include "utils/panic.h"


void _do_irq() {
    panic(__FUNCTION__);
}

void _do_fiq() {
    panic(__FUNCTION__);
}

void _do_swi() {
    panic(__FUNCTION__);
}

void _do_undefined() {
    panic(__FUNCTION__);
}

void _do_pabort() {
    panic(__FUNCTION__);
}

void _do_dabort() {
    panic(__FUNCTION__);
}

void _do_reserved() {
    panic(__FUNCTION__);
}
