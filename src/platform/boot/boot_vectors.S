/**
 ****************************************************************************************
 *
 * @file boot_vectors.S
 *
 * @brief ARM Exception Vectors table.
 *
 * Copyright (C) Beken Corp 2017-2026
 *
 ****************************************************************************************
 */

#include "boot.h"

 
/*
 *************************************************************************
 *
 * Symbol _vector_start is referenced elsewhere, so make it global
 *
 *************************************************************************
 */

	.global _vector_start

/*
 *************************************************************************
 *
 * Vectors have their own section so linker script can map them easily
 *
 *************************************************************************
 */

	.section ".vectors", "ax"

/*
 *************************************************************************
 *
 * Exception vectors as described in ARM reference manuals
 *
 * Uses indirect branch to allow reaching handlers anywhere in memory.
 *
 *************************************************************************
 */
_vector_start:
	b	boot_reset
	ldr	pc, _undefined_instruction
	ldr	pc, _software_interrupt
	ldr	pc, _prefetch_abort
	ldr	pc, _data_abort
	ldr	pc, _not_used
	ldr	pc, _irq
	ldr	pc, _fiq

/*
 *************************************************************************
 *
 * Indirect vectors table
 *
 * Symbols referenced here must be defined somewhere else
 *
 *************************************************************************
 */
_undefined_instruction:	.word boot_undefined
_software_interrupt:	.word boot_swi
_prefetch_abort:	    .word boot_pabort
_data_abort:		    .word boot_dabort
_not_used:			    .word boot_reserved
_irq:				    .word irq_handler
_fiq:				    .word fiq_handler

	.balignl 16,0xdeadbeef


		.align	5
irq_handler:
	STMFD  sp!,{r0-r1}
	LDR    R1, =RAM_VECTORS_IRQ_ADDR
	LDR    r0, [R1]
	BX     r0

		.align	5
fiq_handler:
	STMFD  sp!,{r0-r1}
	LDR    R1, =RAM_VECTORS_FIQ_ADDR
	LDR    r0, [R1]
	BX     r0

		.align	5
boot_swi:
	STMFD  sp!,{r0-r1}
	LDR    R1, =RAM_VECTORS_SWI_ADDR
	LDR    r0, [R1]
	BX     r0

         .align  5
boot_undefined:
 	STMFD  sp!,{r0-r1}
 	LDR    R1, =RAM_VECTORS_UNDEFINED_ADDR
 	LDR    r0, [R1]
 	BX     r0

         .align  5
boot_pabort:
 	STMFD  sp!,{r0-r1}
 	LDR    R1, =RAM_VECTORS_PABORT_ADDR
 	LDR    r0, [R1]
 	BX     r0

         .align  5
boot_dabort:
 	STMFD  sp!,{r0-r1}
 	LDR    R1, =RAM_VECTORS_DABORT_ADDR
 	LDR    r0, [R1]
 	BX     r0

         .align  5
boot_reserved:
 	STMFD  sp!,{r0-r1}
 	LDR    R1, =RAM_VECTORS_RESERVED_ADDR
 	LDR    r0, [R1]
 	BX     r0

/* eof */
