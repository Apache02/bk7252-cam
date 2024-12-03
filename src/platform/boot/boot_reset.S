#include "boot.h"

    .globl _start

/* ========================================================================
 *                                Macros
 * ======================================================================== */
/* ========================================================================
/**
 * Macro for switching ARM mode
 */
	.macro	BOOT_CHANGE_MODE, mode, mode_mask
	MRS   R0, CPSR
	BIC   R0, R0, #\mode_mask
	ORR   R0, R0, #\mode
	MSR   CPSR_c, R0
	.endm

/* ========================================================================
/**
 * Macro for setting the stack
 */
	.macro  BOOT_SET_STACK, stackStart, stackLen, color
	LDR   R0, \stackStart
	LDR   R1, \stackLen

	ADD   R1, R1, R0
	MOV   SP, R1        //Set stack pointer

	LDR   R2, =\color

3:
	CMP   R0, R1        //End of stack?
	STRLT R2, [r0]      //Colorize stack word
	ADDLT R0, R0, #4
	BLT   3b           //branch to previous local label
	.endm

/* ========================================================================
 *                                Globals
 * ======================================================================== */
boot_stack_base_UNUSED:
	.word _stack_unused

boot_stack_len_UNUSED:
	.word _UNUSED_STACK_SIZE_

boot_stack_base_IRQ:
	.word _stack_irq

boot_stack_len_IRQ:
	.word _IRQ_STACK_SIZE_

boot_stack_base_SVC:
   	.word _stack_svc

boot_stack_len_SVC:
  	 .word _SVC_STACK_SIZE_

boot_stack_base_FIQ:
  	 .word _stack_fiq

boot_stack_len_FIQ:
   	.word _FIQ_STACK_SIZE_

boot_stack_base_SYS:
  	 .word _stack_sys

boot_stack_len_SYS:
  	 .word _SYS_STACK_SIZE_


/* ========================================================================
 *                                Functions
 * ========================================================================

/* ========================================================================
 * Function to handle reset vector
 */
	.globl	boot_reset

boot_reset:
    //Disable IRQ and FIQ before starting anything
    MRS   R0, CPSR
    ORR   R0, R0, #0xC0
    MSR   CPSR_c, R0

    //Setup all stacks //Note: Abt and Usr mode are not used
	BOOT_CHANGE_MODE BOOT_MODE_SYS BOOT_MODE_MASK
	BOOT_SET_STACK   boot_stack_base_SYS boot_stack_len_SYS BOOT_COLOR_SYS

	BOOT_CHANGE_MODE BOOT_MODE_ABT BOOT_MODE_MASK
	BOOT_SET_STACK   boot_stack_base_UNUSED boot_stack_len_UNUSED BOOT_COLOR_UNUSED

	BOOT_CHANGE_MODE BOOT_MODE_UND BOOT_MODE_MASK
	BOOT_SET_STACK   boot_stack_base_UNUSED boot_stack_len_UNUSED BOOT_COLOR_UNUSED

	BOOT_CHANGE_MODE BOOT_MODE_IRQ BOOT_MODE_MASK
	BOOT_SET_STACK   boot_stack_base_IRQ boot_stack_len_IRQ BOOT_COLOR_IRQ

	BOOT_CHANGE_MODE BOOT_MODE_FIQ BOOT_MODE_MASK
	BOOT_SET_STACK   boot_stack_base_FIQ boot_stack_len_FIQ BOOT_COLOR_FIQ

	// Clear FIQ banked registers while in FIQ mode
	MOV     R8,  #0
	MOV     R9,  #0
	MOV     R10, #0
	MOV     R11, #0
	MOV     R12, #0

	BOOT_CHANGE_MODE BOOT_MODE_SVC BOOT_MODE_MASK
	BOOT_SET_STACK   boot_stack_base_SVC boot_stack_len_SVC BOOT_COLOR_SVC

    // Stay in Supervisor Mode
    // copy data from binary to ram
    BL _sysboot_copy_data_to_ram
    
    // Init the BSS section
    BL _sysboot_zi_init
    
    //==================
    //Clear Registers
    MOV R0, #0
    MOV R1, #0
    MOV R2, #0
    MOV R3, #0
    MOV R4, #0
    MOV R5, #0
    MOV R6, #0
    MOV R7, #0
    MOV R8, #0
    MOV R9, #0
    MOV R10, #0
    MOV R11, #0
    MOV R12, #0

    B _start

/*FUNCTION:     _sysboot_copy_data_to_ram*/
/*DESCRIPTION:  copy main stack code from FLASH/ROM to SRAM*/
_sysboot_copy_data_to_ram:
    LDR     R0, =_data_flash_begin
    LDR     R1, =_data_ram_begin
    LDR     R2, =_data_ram_end
	
4: CMP R1, R2
    LDRLO   R4, [R0], #4
    STRLO   R4, [R1], #4
    BLO     4b
    BX LR
        
/*FUNCTION:     _sysboot_zi_init*/
/*DESCRIPTION:  Initialise Zero-Init Data Segment*/
_sysboot_zi_init:
    LDR     R0, =_bss_start
    LDR     R1, =_bss_end
    
    MOV R3, R1
    MOV R4, R0
    MOV R2, #0
5: CMP R4, R3
    STRLO R2, [R4], #4
    BLO 5b
    BX LR
