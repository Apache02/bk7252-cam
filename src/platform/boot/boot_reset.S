#include "boot.h"

.syntax unified
.arm

/////////////////// MACROS ///////////////////

.macro  BACKUP_REGISTERS
    mov   r10, r3
    ldr   r3, =(boot_entry_state + 64)
    stmdb r3!, {r4-r12, sp, lr}
    stmdb r3!, {r10}
    stmdb r3!, {r0-r2}
    mrs   r0, cpsr
    stmdb r3!, {r0}
.endm

.macro	DISABLE_IRQ_FIQ
    mrs   r0, cpsr
    orr   r0, r0, #CPU_FIQ_IRQ_MASK
    msr   cpsr_c, r0
.endm

.macro	BOOT_CHANGE_MODE, mode, mode_mask
    mrs   r0, cpsr
    bic   r0, r0, #\mode_mask
    orr   r0, r0, #\mode
    msr   cpsr_c, r0
.endm

.macro  BOOT_SET_STACK, stackStart, stackLen, color
    ldr   r0, =\stackStart
    ldr   r2, =\stackLen
    add   r1, r0, r2
    mov   sp, r1
    ldr   r2, =\color
loc_stack_loop_\@:
    cmp   r0, r1
    strlt r2, [r0]
    addlt r0, r0, #4
    blt   loc_stack_loop_\@
.endm

/////////////////// RESET HANDLER ///////////////////

.global _reset
.global _start

.text
.arm
_reset:
#ifdef ENTRY_BACKUP_REGISTERS
    BACKUP_REGISTERS
#endif
    DISABLE_IRQ_FIQ

    // Setup all stacks // Note: Abt and Usr mode are not used

    BOOT_CHANGE_MODE CPU_MODE_SYS, CPU_MODE_MASK
    BOOT_SET_STACK _stack_sys, _SYS_STACK_SIZE_, BOOT_COLOR_SYS

    BOOT_CHANGE_MODE CPU_MODE_ABT, CPU_MODE_MASK
    BOOT_SET_STACK _stack_unused, _UNUSED_STACK_SIZE_, BOOT_COLOR_UNUSED

    BOOT_CHANGE_MODE CPU_MODE_UND, CPU_MODE_MASK
    BOOT_SET_STACK _stack_unused, _UNUSED_STACK_SIZE_, BOOT_COLOR_UNUSED

    BOOT_CHANGE_MODE CPU_MODE_IRQ, CPU_MODE_MASK
    BOOT_SET_STACK _stack_irq, _IRQ_STACK_SIZE_, BOOT_COLOR_IRQ

    BOOT_CHANGE_MODE CPU_MODE_FIQ, CPU_MODE_MASK
    BOOT_SET_STACK _stack_fiq, _FIQ_STACK_SIZE_, BOOT_COLOR_FIQ

    // Clear FIQ banked registers while in FIQ mode
    mov   r8, #0
    mov   r9, #0
    mov   r10, #0
    mov   r11, #0
    mov   r12, #0

    BOOT_CHANGE_MODE CPU_MODE_SVC, CPU_MODE_MASK
    BOOT_SET_STACK _stack_svc, _SVC_STACK_SIZE_, BOOT_COLOR_SVC

    // Stay in Supervisor Mode
    bl _sysboot_copy_data_to_ram
    bl _sysboot_zi_init

    // Clear Registers
    mov   r0, #0
    mov   r1, #0
    mov   r2, #0
    mov   r3, #0
    mov   r4, #0
    mov   r5, #0
    mov   r6, #0
    mov   r7, #0
    mov   r8, #0
    mov   r9, #0
    mov   r10, #0
    mov   r11, #0
    mov   r12, #0

    b     _start

