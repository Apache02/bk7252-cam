	.globl intc_irq
	.globl intc_fiq
	.globl do_irq
	.globl do_fiq
	.globl do_swi
	.globl do_undefined
	.globl do_pabort
	.globl do_dabort
	.globl do_reserved
    .globl bk_trap_swi
    .globl bk_trap_udef
    .globl bk_trap_pabt
    .globl bk_trap_dabt
    .globl bk_trap_resv


/* ========================================================================
 Context save and restore macro definitions
 * ======================================================================== */
/* ========================================================================
/**
 * Push SVC reg
 */
    .macro PUSH_SVC_REG
        SUB     SP, SP, #17 * 4
        STMIA   SP, {R0 - R12}
        MOV     R0, SP
        MRS     R6, SPSR
        STR     LR, [R0, #15*4]
        STR     R6, [R0, #16*4]
        STR     SP, [R0, #13*4]
        STR     LR, [R0, #14*4]
    .endm
    
/* ========================================================================*/
	.macro   portSAVE_CONTEXT 
	// Push R0 as we are going to use the register.
	STMDB	SP!, {R0}	
	
	LDMIA  SP!, {R0}
	
	STMDB	SP!, {r0-r12,r14}
	NOP
	NOP
	SUB SP, SP, #8

	MOV R0, R0
	.endm
		
/* ========================================================================*/
	.macro   portRESTORE_CONTEXT 
		
	ADD SP, SP,#0x8

	LDMIA	SP!, {r0-r12,r14}
	NOP
	NOP	
		
	SUBS    PC, LR, #0x4
	.endm

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
    PUSH_SVC_REG
    B bk_trap_swi

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


/*eof*/
