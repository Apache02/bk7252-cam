#ifndef _HARDWARE_JPEG_H
#define _HARDWARE_JPEG_H

#include "hardware/icu.h"


#ifdef __cplusplus
extern "C" {
#endif

#define jpeg_power_up()   icu_jpeg_power_up()
#define jpeg_power_down() icu_jpeg_power_down()

void jpeg_init_quant_table();

void jpeg_init();

#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_JPEG_H
