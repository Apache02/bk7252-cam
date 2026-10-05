#pragma once

#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

void intc_service_register(uint8_t int_num, uint8_t int_pri, void (*isr)(void));

#ifdef __cplusplus
}
#endif
