#include "drivers/chip_id.h"


struct chip_and_device_id_t {
    uint32_t chip_id;
    uint32_t device_id;
};

#define chip_and_device_id  ((struct chip_and_device_id_t *)0x00800000)

uint32_t chip_id() {
    return chip_and_device_id->chip_id;
}

uint32_t device_id() {
    return chip_and_device_id->device_id;
}