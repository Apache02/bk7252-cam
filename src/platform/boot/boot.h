#ifndef PLATFORM_BOOT_H
#define PLATFORM_BOOT_H

#define _FIQ_STACK_SIZE_                0x7F0
#define _IRQ_STACK_SIZE_                0xFF0
#define _SVC_STACK_SIZE_                0x3F0
#define _SYS_STACK_SIZE_                0x3F0
#define _UNUSED_STACK_SIZE_             0x010

#define BOOT_COLOR_UNUSED               0xAAAAAAAA      // Pattern to fill UNUSED stack
#define BOOT_COLOR_SVC                  0xBBBBBBBB      // Pattern to fill SVC stack
#define BOOT_COLOR_IRQ                  0xCCCCCCCC      // Pattern to fill IRQ stack
#define BOOT_COLOR_FIQ                  0xDDDDDDDD      // Pattern to fill FIQ stack
#define BOOT_COLOR_SYS                  0xEEEEEEEE      // Pattern to fill SYS stack

#include "platform/arm.h"

#define RAM_VECTORS_IRQ_ADDR            0x00400000
#define RAM_VECTORS_FIQ_ADDR            0x00400004
#define RAM_VECTORS_SWI_ADDR            0x00400008
#define RAM_VECTORS_UNDEFINED_ADDR      0x0040000C
#define RAM_VECTORS_PABORT_ADDR         0x00400010
#define RAM_VECTORS_DABORT_ADDR         0x00400014
#define RAM_VECTORS_RESERVED_ADDR       0x00400018

#endif // PLATFORM_BOOT_H
