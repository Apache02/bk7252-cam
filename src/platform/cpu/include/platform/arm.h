#pragma once

#define CPU_MODE_MASK                   0x1F
#define CPU_MODE_USR                    0x10
#define CPU_MODE_FIQ                    0x11
#define CPU_MODE_IRQ                    0x12
#define CPU_MODE_SVC                    0x13
#define CPU_MODE_ABT                    0x17
#define CPU_MODE_UND                    0x1B
#define CPU_MODE_SYS                    0x1F

#define CPU_FIQ_MASK                    0x40
#define CPU_IRQ_MASK                    0x80
#define CPU_FIQ_IRQ_MASK                0xC0

