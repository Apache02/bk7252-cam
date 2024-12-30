#ifndef _HARDWARE_RANDOM_H
#define _HARDWARE_RANDOM_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/cdefs.h>


#ifdef __cplusplus
extern "C" {
#endif


void trng_enable();

void trng_disable();

uint32_t get_random();


#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_RANDOM_H
