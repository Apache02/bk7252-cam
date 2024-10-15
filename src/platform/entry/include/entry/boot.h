#ifndef PLATFORM_ENTRY_ARCH_H
#define PLATFORM_ENTRY_ARCH_H

#define _FIQ_STACK_SIZE_ 	              0x7F0
#define _IRQ_STACK_SIZE_ 	              0xFF0
#define _SVC_STACK_SIZE_ 	              0x3F0
#define _SYS_STACK_SIZE_ 	              0x3F0
#define _UNUSED_STACK_SIZE_ 	          0x010

#define BOOT_MODE_MASK     	              0x1F
#define BOOT_MODE_USR       	          0x10
#define BOOT_MODE_FIQ       	          0x11
#define BOOT_MODE_IRQ      	              0x12
#define BOOT_MODE_SVC    	              0x13
#define BOOT_MODE_ABT     	              0x17
#define BOOT_MODE_UND    	              0x1B
#define BOOT_MODE_SYS     	              0x1F
#define BOOT_FIQ_IRQ_MASK 	              0xC0
#define BOOT_IRQ_MASK       	          0x80

#define BOOT_COLOR_UNUSED  	              0xAAAAAAAA      //Pattern to fill UNUSED stack
#define BOOT_COLOR_SVC     	              0xBBBBBBBB      //Pattern to fill SVC stack
#define BOOT_COLOR_IRQ     	              0xCCCCCCCC      //Pattern to fill IRQ stack
#define BOOT_COLOR_FIQ     	              0xDDDDDDDD      //Pattern to fill FIQ stack
#define BOOT_COLOR_SYS     	              0xEEEEEEEE      //Pattern to fill SYS stack


#endif // PLATFORM_ENTRY_ARCH_H
