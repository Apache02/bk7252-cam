#include "chip_id.h"


#define SCTRL_BASE              (0x00800000)


struct chip_and_device_id_t {
    uint32_t chip_id;
    uint32_t device_id;
};

#define chip_and_device_id  ((volatile struct chip_and_device_id_t *)SCTRL_BASE)

uint32_t chip_id() {
    return chip_and_device_id->chip_id;
}

uint32_t device_id() {
    return chip_and_device_id->device_id;
}
