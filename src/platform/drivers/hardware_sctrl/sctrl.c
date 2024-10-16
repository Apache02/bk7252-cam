#include "sctrl_regs.h"

uint32_t chip_id() {
    return hw_sctrl->chip_id;
}

uint32_t device_id() {
    return hw_sctrl->device_id;
}
