#ifndef _HARDWARE_SCTRL_H
#define _HARDWARE_SCTRL_H

#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

uint32_t chip_id();

uint32_t device_id();

#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_SCTRL_H
