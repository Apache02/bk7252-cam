#include <stdint.h>
#include "platform/stacks.h"

/* Placement (top of RAM, order) comes from the .stacks output section in the
   linker script; sizes come from the same platform/stacks.h macros
   boot_reset.S uses to paint and bound each mode's stack. */

__attribute__((section(".stack.unused"), used)) uint8_t _stack_unused[_UNUSED_STACK_SIZE_];
__attribute__((section(".stack.svc"), used)) uint8_t _stack_svc[_SVC_STACK_SIZE_];
__attribute__((section(".stack.irq"), used)) uint8_t _stack_irq[_IRQ_STACK_SIZE_];
__attribute__((section(".stack.fiq"), used)) uint8_t _stack_fiq[_FIQ_STACK_SIZE_];
__attribute__((section(".stack.sys"), used)) uint8_t _stack_sys[_SYS_STACK_SIZE_];
