    .global ulCriticalNesting
    .global  pxCurrentTCB

; /**********************************************************************/

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

; /**********************************************************************/

.macro portRESTORE_CONTEXT

    LDR     R0, =pxCurrentTCB       // Set the LR to the task stack.  The location was...
    LDR     R0, [R0]                // ... stored in pxCurrentTCB
    LDR     LR, [R0]

    LDR     R0, =ulCriticalNesting  // The critical nesting depth is the first item on...
    LDMFD   LR!, {R1}               // ...the stack.  Load it into the ulCriticalNesting var.
    STR     R1, [R0]                //

    LDMFD   LR!, {R0}               // Get the SPSR from the stack.
    MSR     SPSR_cxsf, R0           //

    LDMFD   LR, {R0-R14}^           // Restore all system mode registers for the task.
    NOP                             //

    LDR     LR, [LR, #+60]          // Restore the return address

                                    // And return - correcting the offset in the LR to obtain ...
    SUBS    PC, LR, #4              // ...the correct address.

.endm

; /**********************************************************************/

.macro portSAVE_CONTEXT

    STMDB   SP!, {R0}               // Store R0 first as we need to use it.

    STMDB   SP,{SP}^                // Set R0 to point to the task stack pointer.
    NOP                             //
    SUB     SP, SP, #4              //
    LDMIA   SP!,{R0}                //

    STMDB   R0!, {LR}               // Push the return address onto the stack.
    MOV     LR, R0                  // Now we have saved LR we can use it instead of R0.
    LDMIA   SP!, {R0}               // Pop R0 so we can save it onto the system mode stack.

    STMDB   LR,{R0-LR}^             // Push all the system mode registers onto the task stack.
    NOP                             //
    SUB     LR, LR, #60             //

    MRS     R0, SPSR                // Push the SPSR onto the task stack.
    STMDB   LR!, {R0}               //

    LDR     R0, =ulCriticalNesting  //
    LDR     R0, [R0]                //
    STMDB   LR!, {R0}               //

    LDR     R0, =pxCurrentTCB       // Store the new top of stack for the task.
    LDR     R1, [R0]                //
    STR     LR, [R1]                //

.endm
