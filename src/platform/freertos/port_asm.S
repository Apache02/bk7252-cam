.syntax unified
.arm

.text

/////////////////// GLOBALS ///////////////////

// FreeRTOS state (defined in tasks.c)
.global ulCriticalNesting
.global pxCurrentTCB

// Exports
.global do_irq
.global do_fiq
.global do_swi
.global do_undefined
.global do_pabort
.global do_dabort
.global do_reserved

.global vPortStartFirstTask
.global vPortYield

// Imports
.global intc_irq
.global intc_fiq
.global bk_trap_swi
.global bk_trap_udef
.global bk_trap_pabt
.global bk_trap_dabt
.global bk_trap_resv

.global vTaskSwitchContext

/////////////////// MACROS ///////////////////

.macro PUSH_SVC_REG
    sub     sp, sp, #17 * 4
    stmia   sp, {r0 - r12}
    mov     r0, sp
    mrs     r6, spsr
    str     lr, [r0, #15*4]
    str     r6, [r0, #16*4]
    str     sp, [r0, #13*4]
    str     lr, [r0, #14*4]
.endm

.macro PORT_SAVE_CONTEXT
    stmdb   sp!, {r0}               // Store r0 first as we need to use it.

    stmdb   sp, {sp}^               // Store user-mode sp so we can load it into r0.
    nop
    sub     sp, sp, #4
    ldmia   sp!, {r0}               // r0 = user-mode (task) sp.

    stmdb   r0!, {lr}               // Push return address onto task stack.
    mov     lr, r0                  // lr now holds the task sp; r0 is free.
    ldmia   sp!, {r0}               // Pop r0 to save it onto the system mode stack.

    stmdb   lr, {r0-r14}^           // Push all system mode registers onto task stack.
    nop
    sub     lr, lr, #60

    mrs     r0, spsr                // Push SPSR onto task stack.
    stmdb   lr!, {r0}

    ldr     r0, =ulCriticalNesting
    ldr     r0, [r0]
    stmdb   lr!, {r0}

    ldr     r0, =pxCurrentTCB       // Store new top of stack for the task.
    ldr     r1, [r0]
    str     lr, [r1]
.endm

.macro PORT_RESTORE_CONTEXT
    ldr     r0, =pxCurrentTCB       // Load task stack pointer from pxCurrentTCB.
    ldr     r0, [r0]
    ldr     lr, [r0]

    ldr     r0, =ulCriticalNesting  // Critical nesting depth is first item on stack.
    ldmfd   lr!, {r1}
    str     r1, [r0]

    ldmfd   lr!, {r0}               // Get SPSR from stack.
    msr     spsr_cxsf, r0

    ldmfd   lr, {r0-r14}^           // Restore all system mode registers.
    nop

    ldr     lr, [lr, #60]           // Restore return address.

    subs    pc, lr, #4              // Return, correcting lr offset.
.endm

/////////////////// EXCEPTION HANDLERS ///////////////////

.align 4
.type do_irq, %function
do_irq:
    ldmfd   sp!, {r0-r1}
    PORT_SAVE_CONTEXT
    ldr     r0, =intc_irq
    mov     lr, pc
    bx      r0
    PORT_RESTORE_CONTEXT

.align 4
.type do_fiq, %function
do_fiq:
    ldmfd   sp!, {r0-r1}
    PORT_SAVE_CONTEXT
    ldr     r0, =intc_fiq
    mov     lr, pc
    bx      r0
    PORT_RESTORE_CONTEXT

.align 4
.type do_swi, %function
do_swi:
    ldmfd   sp!, {r0-r1}
    b       vPortYieldProcessor

.align 4
.type do_undefined, %function
do_undefined:
    ldmfd   sp!, {r0-r1}
    PUSH_SVC_REG
    b       bk_trap_udef

.align 4
.type do_pabort, %function
do_pabort:
    ldmfd   sp!, {r0-r1}
    PUSH_SVC_REG
    b       bk_trap_pabt

.align 4
.type do_dabort, %function
do_dabort:
    ldmfd   sp!, {r0-r1}
    PUSH_SVC_REG
    b       bk_trap_dabt

.align 4
.type do_reserved, %function
do_reserved:
    ldmfd   sp!, {r0-r1}
    PUSH_SVC_REG
    b       bk_trap_resv

/////////////////// FREERTOS PORT ///////////////////

.align 4
.type vPortStartFirstTask, %function
vPortStartFirstTask:
    PORT_RESTORE_CONTEXT

.align 4
.type vPortYield, %function
vPortYield:
    svc     0
    bx      lr

.align 4
vPortYieldProcessor:
    add     lr, lr, #4              // Adjust lr to appear as if saved during an IRQ handler.
    PORT_SAVE_CONTEXT
    ldr     r0, =vTaskSwitchContext
    mov     lr, pc
    bx      r0
    PORT_RESTORE_CONTEXT
