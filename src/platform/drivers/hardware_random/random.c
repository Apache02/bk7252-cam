#include "hardware/random.h"
#include "random_regs.h"

void trng_enable() {
    hw_trng->ctrl.enable = 1;
    get_random();
}

void trng_disable() {
    hw_trng->ctrl.enable = 1;
    hw_trng->data = 0x1234;
}

uint32_t get_random() {
    return hw_trng->data;
}

