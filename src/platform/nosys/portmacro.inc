; /**********************************************************************/

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

; /**********************************************************************/

.macro portSAVE_CONTEXT

    STMDB   SP!, {R0}               // Store R0 first as we need to use it.

    LDMIA   SP!, {R0}

    STMDB	SP!, {r0-r12,r14}
    NOP
    NOP
    SUB SP, SP, #8

    MOV R0, R0
.endm

; /**********************************************************************/

.macro portRESTORE_CONTEXT

    ADD SP, SP,#0x8

    LDMIA	SP!, {r0-r12,r14}
    NOP
    NOP

    SUBS    PC, LR, #4
.endm
