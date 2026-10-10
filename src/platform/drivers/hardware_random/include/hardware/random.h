#ifndef _HARDWARE_RANDOM_H
#define _HARDWARE_RANDOM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

void trng_enable();

void trng_disable();

// A 32-bit word mixed from 17 reads of the TRNG. The TRNG must be enabled.
uint32_t get_random();

// Fills `len` bytes. The TRNG must be enabled.
void get_random_bytes(void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_RANDOM_H
