#ifndef _PLATFORM_STACKS_H
#define _PLATFORM_STACKS_H

#define _FIQ_STACK_SIZE_    0x7F0
#define _IRQ_STACK_SIZE_    0xFF0
#define _SVC_STACK_SIZE_    0x3F0
#define _SYS_STACK_SIZE_    0x3F0
#define _UNUSED_STACK_SIZE_ 0x010

#define _STACK_TOTAL_SIZE_ (_UNUSED_STACK_SIZE_ + _SVC_STACK_SIZE_ + _IRQ_STACK_SIZE_ + _FIQ_STACK_SIZE_ + _SYS_STACK_SIZE_)

#define STACK_COLOR_UNUSED 0xAAAAAAAA // Pattern to fill UNUSED stack
#define STACK_COLOR_SVC    0xBBBBBBBB // Pattern to fill SVC stack
#define STACK_COLOR_IRQ    0xCCCCCCCC // Pattern to fill IRQ stack
#define STACK_COLOR_FIQ    0xDDDDDDDD // Pattern to fill FIQ stack
#define STACK_COLOR_SYS    0xEEEEEEEE // Pattern to fill SYS stack

// This header is also run through the plain preprocessor for the .lds
// scripts and boot_reset.S (both build with -x assembler-with-cpp, which
// predefines __ASSEMBLER__), so the symbol declarations below — meaningless
// to ld/gas — must stay out of that pass.
#ifndef __ASSEMBLER__
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The per-mode stacks (defined in stacks.c, placed by src/linker/*.lds) and
// the _stack_begin/_stack_end symbols bounding the whole reserved region.
// Use _stack_begin/_stack_end (not _stack_unused) as "where does the stack
// area start/end".
extern uint8_t _stack_begin;
extern uint8_t _stack_unused[_UNUSED_STACK_SIZE_];
extern uint8_t _stack_svc[_SVC_STACK_SIZE_];
extern uint8_t _stack_irq[_IRQ_STACK_SIZE_];
extern uint8_t _stack_fiq[_FIQ_STACK_SIZE_];
extern uint8_t _stack_sys[_SYS_STACK_SIZE_];
extern uint8_t _stack_end;

#ifdef __cplusplus
}
#endif
#endif // __ASSEMBLER__

#endif // _PLATFORM_STACKS_H
