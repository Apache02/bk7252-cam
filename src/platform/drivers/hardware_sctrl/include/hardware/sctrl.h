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

// Powers up MAC and modem and enables the radio controller. WiFi applications
// only — sctrl_init() deliberately leaves those blocks alone.
void sctrl_rf_init();

bool     sctrl_set_cpu_freq_hz(uint32_t freq);
uint32_t sctrl_get_cpu_freq_hz();

// The analog_ctrl* registers reach the analog die over an internal SPI link;
// every access has to wait for the previous transfer to retire.
void     sctrl_analog_set(volatile uint32_t *reg, uint32_t value);
uint32_t sctrl_analog_get(volatile const uint32_t *reg);

void sctrl_cali_dpll(void);

void sctrl_dpll_int_open(void);
void sctrl_dpll_int_close(void);

void sctrl_subsys_modem_reset(void);
void sctrl_subsys_mac_reset(void);
void sctrl_subsys_usb_reset(void);
void sctrl_subsys_dsp_reset(void);

void sctrl_modem_core_reset(void);
void sctrl_overclock(bool enable);

#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_SCTRL_H
