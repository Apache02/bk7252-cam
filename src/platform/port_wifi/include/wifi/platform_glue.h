#pragma once

#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

void intc_service_register(uint8_t int_num, uint8_t int_pri, void (*isr)(void));

// Tells the LMAC whether the association it builds next is for a secured
// network: bk_sta_cipher_is_open() answers from this, and the Privacy bit of the
// request follows it.
void wifi_glue_set_sta_secure(bool secure);

#ifdef __cplusplus
}
#endif
