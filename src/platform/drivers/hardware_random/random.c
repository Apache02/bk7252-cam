#include "hardware/random.h"
#include "random_regs.h"
#include "utils/busy_wait.h"

void trng_enable() {
    hw_trng->ctrl.enable = 1;

    busy_wait_us(32); // time to accumulate entropy?
}

void trng_disable() {
    hw_trng->ctrl.enable = 0;
    hw_trng->data = 0x1234;
}

uint32_t get_random() {
    return hw_trng->data;
}

