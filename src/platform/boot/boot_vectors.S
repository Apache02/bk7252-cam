#include "boot.h"

.syntax unified
.arm

.section ".vectors", "ax"
.global _vector_start

_vector_start:
    b _reset
    ldr pc, _undef_addr
    ldr pc, _swi_addr
    ldr pc, _pabort_addr
    ldr pc, _dabort_addr
    ldr pc, _reserved_addr
    ldr pc, _irq_addr
    ldr pc, _fiq_addr

_undef_addr:            .word boot_undef
_swi_addr:              .word boot_swi
_pabort_addr:           .word boot_pabort
_dabort_addr:           .word boot_dabort
_reserved_addr:         .word boot_reserved
_irq_addr:              .word boot_irq
_fiq_addr:              .word boot_fiq

.balignl 16, 0xdeadbeef

/////////////////// HANDLERS ///////////////////

.macro  RAM_VECTOR_HANDLER, addr
    push   {r0, r1}
    ldr    r1, =\addr
    ldr    r1, [r1]
    bx     r1
.endm

.arm

.align 4
boot_undef:
    RAM_VECTOR_HANDLER RAM_VECTORS_UNDEFINED_ADDR

.align 4
boot_swi:
    RAM_VECTOR_HANDLER RAM_VECTORS_SWI_ADDR

.align 4
boot_pabort:
    RAM_VECTOR_HANDLER RAM_VECTORS_PABORT_ADDR

.align 4
boot_dabort:
    RAM_VECTOR_HANDLER RAM_VECTORS_DABORT_ADDR

.align 4
boot_reserved:
    RAM_VECTOR_HANDLER RAM_VECTORS_RESERVED_ADDR

.align 4
boot_irq:
    RAM_VECTOR_HANDLER RAM_VECTORS_IRQ_ADDR

.align 4
boot_fiq:
    RAM_VECTOR_HANDLER RAM_VECTORS_FIQ_ADDR

/////////////////// END ///////////////////
