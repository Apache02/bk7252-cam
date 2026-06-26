#ifndef _HARDWARE_SCTRL_H
#define _HARDWARE_SCTRL_H

#include <stdbool.h>
#include <stdint.h>


#define CPU_FREQ_160_MHZ (160 * 1000 * 1000)
#define CPU_FREQ_120_MHZ (120 * 1000 * 1000)
#define CPU_FREQ_96_MHZ  (96 * 1000 * 1000)
#define CPU_FREQ_80_MHZ  (80 * 1000 * 1000)
#define CPU_FREQ_60_MHZ  (60 * 1000 * 1000)
#define CPU_FREQ_48_MHZ  (48 * 1000 * 1000)
#define CPU_FREQ_40_MHZ  (40 * 1000 * 1000)
#define CPU_FREQ_32_MHZ  (32 * 1000 * 1000)
#define CPU_FREQ_30_MHZ  (30 * 1000 * 1000)
#define CPU_FREQ_26_MHZ  (26 * 1000 * 1000)
#define CPU_FREQ_13_MHZ  (13 * 1000 * 1000)

#define DEFAULT_CPU_FREQ CPU_FREQ_120_MHZ


#ifdef __cplusplus
extern "C" {
#endif

uint32_t chip_id();

uint32_t device_id();

void sctrl_init();

bool sctrl_set_cpu_freq_hz(uint32_t freq);

uint32_t sctrl_get_cpu_freq_hz();

#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_SCTRL_H
