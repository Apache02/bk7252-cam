#include "platform/arm.h"

.syntax unified
.arm

.text

/////////////////// GLOBALS ///////////////////

// FreeRTOS state (defined in tasks.c)
.global ulCriticalNesting
.global pxCurrentTCB
.global xPortPendingContextSwitch

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

// The task-context path below is only valid when the interrupted context was a
// task, i.e. System mode (portINITIAL_SPSR). An exception taken on top of
// another exception must not go through it: ARM masks only I on IRQ entry, so a
// FIQ can land in the middle of do_irq, and intc_fiq() reaches
// vTaskSwitchContext() through portYIELD_FROM_ISR() whenever a MAC ISR posts to
// a queue. Saving to the task stack and committing pxCurrentTCB from there
// would make the inner handler resume the *newly selected* task's registers and
// stack inside the outer handler.
//
// So when SPSR says we interrupted a handler, save to the current mode's own
// stack and leave pxCurrentTCB alone — the outer handler's portRESTORE_CONTEXT
// performs the pending switch. This mirrors the vendor port
// (beken378/driver/entry/boot_handlers.S).
.macro portSAVE_CONTEXT
    stmdb   sp!, {r0}               // Store r0 first as we need to use it.

    mrs     r0, spsr                // Did we interrupt a task, or a handler?
    and     r0, r0, #CPU_MODE_MASK
    cmp     r0, #CPU_MODE_SYS
    bne     nested_save_\@

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

    b       save_done_\@

nested_save_\@:
    ldmia   sp!, {r0}               // Undo the r0 push; the whole frame goes on
                                    // this mode's stack instead.
    stmdb   sp!, {r0-r12, lr}       // In FIQ mode r8-r12 are banked, so these are
    nop                             // the handler's own — the interrupted
                                    // context's copies are untouched either way.

    mrs     r0, spsr
    stmdb   sp!, {r0}

    ldr     r0, =ulCriticalNesting
    ldr     r0, [r0]
    stmdb   sp!, {r0}

    sub     sp, sp, #4              // Padding word, popped by portRESTORE_CONTEXT.

save_done_\@:
.endm

// Resume whatever pxCurrentTCB points at. Used both as the task branch of
// portRESTORE_CONTEXT and standalone by vPortStartFirstTask, which has no
// meaningful SPSR to test yet.
.macro portRESTORE_TASK_CONTEXT
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

.macro portRESTORE_CONTEXT
    mrs     r0, spsr                // Must match the branch portSAVE_CONTEXT took.
    and     r0, r0, #CPU_MODE_MASK
    cmp     r0, #CPU_MODE_SYS
    bne     nested_restore_\@

    portRESTORE_TASK_CONTEXT

nested_restore_\@:
    add     sp, sp, #4              // Drop the padding word.

    ldr     r0, =ulCriticalNesting
    ldmfd   sp!, {r1}
    str     r1, [r0]

    ldmfd   sp!, {r0}
    msr     spsr_cxsf, r0

    ldmia   sp!, {r0-r12, lr}
    nop

    subs    pc, lr, #4              // Resume the interrupted handler.
.endm

// Consume a switch requested by portEND_SWITCHING_ISR(), but only when SPSR says
// we interrupted a task — from a nested handler this would move pxCurrentTCB out
// from under the handler we are about to return into. Runs after the C handler
// and before portRESTORE_CONTEXT, so r0/r1 are free scratch: the context is
// already saved and portRESTORE_CONTEXT reloads everything.
.macro portAPPLY_PENDING_SWITCH
    mrs     r0, spsr
    and     r0, r0, #CPU_MODE_MASK
    cmp     r0, #CPU_MODE_SYS
    bne     no_pending_switch_\@

    ldr     r0, =xPortPendingContextSwitch
    ldr     r1, [r0]
    cmp     r1, #0
    beq     no_pending_switch_\@

    mov     r1, #0
    str     r1, [r0]

    ldr     r0, =vTaskSwitchContext
    mov     lr, pc
    bx      r0

no_pending_switch_\@:
.endm

/////////////////// EXCEPTION HANDLERS ///////////////////

.align 4
.type do_irq, %function
do_irq:
    ldmfd   sp!, {r0-r1}
    portSAVE_CONTEXT
    ldr     r0, =intc_irq
    mov     lr, pc
    bx      r0
    portAPPLY_PENDING_SWITCH
    portRESTORE_CONTEXT

.align 4
.type do_fiq, %function
do_fiq:
    ldmfd   sp!, {r0-r1}
    portSAVE_CONTEXT
    ldr     r0, =intc_fiq
    mov     lr, pc
    bx      r0
    portAPPLY_PENDING_SWITCH
    portRESTORE_CONTEXT

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
    portRESTORE_TASK_CONTEXT

.align 4
.type vPortYield, %function
vPortYield:
    svc     0
    bx      lr

.align 4
vPortYieldProcessor:
    add     lr, lr, #4              // Adjust lr to appear as if saved during an IRQ handler.
    portSAVE_CONTEXT
    ldr     r0, =vTaskSwitchContext
    mov     lr, pc
    bx      r0
    portRESTORE_CONTEXT
