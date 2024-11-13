#include "portmacro.inc"

;/* exports */
	.global do_irq
	.global do_fiq
	.global do_swi
	.global do_undefined
	.global do_pabort
	.global do_dabort
	.global do_reserved

;/* imports */
	.global intc_irq
	.global intc_fiq
    .global bk_trap_swi
    .global bk_trap_udef
    .global bk_trap_pabt
    .global bk_trap_dabt
    .global bk_trap_resv
    .global vTaskSwitchContext

    .align  4

/* ========================================================================
 Context save and restore macro definitions
 * ======================================================================== */
/* ========================================================================

/* ========================================================================
 *                                Functions
 * ======================================================================== */

		.align	5
do_irq:
    LDMFD   SP!, {R0-R1}
	portSAVE_CONTEXT
	LDR R0, =intc_irq
	MOV LR, PC
	BX R0
	portRESTORE_CONTEXT

		.align	5
do_fiq:
    LDMFD   SP!, {R0-R1}
	portSAVE_CONTEXT
	LDR R0, =intc_fiq
	MOV LR, PC
	BX R0
	portRESTORE_CONTEXT

		.align	5
do_swi:
    LDMFD   SP!, {R0-R1}
    B vPortYieldProcessor

		.align	5
do_undefined:
    LDMFD   SP!, {R0-R1}
    PUSH_SVC_REG
    B bk_trap_udef

		.align	5
do_pabort:
    LDMFD   SP!, {R0-R1}
    PUSH_SVC_REG
    B bk_trap_pabt

		.align	5
do_dabort:
    LDMFD   SP!, {R0-R1}
    PUSH_SVC_REG
    B bk_trap_dabt

		.align	5
do_reserved:
    LDMFD   SP!, {R0-R1}
    PUSH_SVC_REG
    B bk_trap_resv

    .text
    .align  4

    .globl vPortStartFirstTask
    .type vPortStartFirstTask, %function
vPortStartFirstTask:
    portRESTORE_CONTEXT


    .globl vPortYield
    .type vPortYield, %function
vPortYield:
    SVC 0
    BX LR


vPortYieldProcessor:
	ADD		LR, LR, #4			// Add 4 to the LR to make the LR appear exactly
								// as if the context was saved during and IRQ
								// handler.

	portSAVE_CONTEXT			// Save the context of the current task...

	LDR R0, =vTaskSwitchContext
	MOV LR, PC
	BX R0

	portRESTORE_CONTEXT			// Restore the context of the selected task.


/*eof*/
