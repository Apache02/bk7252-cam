#include "arm.h"

.text
    .align  4

.code 32

/* ************************ */

/*
 *  int portENABLE_IRQ( void );
 */
    .global portENABLE_IRQ
    .type portENABLE_IRQ, %function
portENABLE_IRQ:
    MRS     R0, CPSR
    BIC     R1, R0, #CPU_IRQ_MASK
    MSR     CPSR_c, R1
    BX      LR

/*
 * int portDISABLE_IRQ( void );
 */
    .global portDISABLE_IRQ
    .type portDISABLE_IRQ, %function
portDISABLE_IRQ:
    MRS     R2, CPSR
    ORR     R1, R2, #CPU_IRQ_MASK
    MSR     CPSR_c, R1
    AND     R0, R2, #CPU_IRQ_MASK
    BX      LR


/*
 * int portENABLE_FIQ( void );
 */
    .global portENABLE_FIQ
    .type portENABLE_FIQ, %function
portENABLE_FIQ:
    MRS     R0, CPSR
    BIC     R1, R0, #CPU_FIQ_MASK
    MSR     CPSR_c, R1
    BX      LR

/*
 * int portDISABLE_FIQ( void );
 */
    .global portDISABLE_FIQ
    .type portDISABLE_FIQ, %function
portDISABLE_FIQ:
    MRS     R2, CPSR
    ORR     R1, R2, #CPU_FIQ_MASK
    MSR     CPSR_c, R1
    AND     R0, R2, #CPU_FIQ_MASK
    BX      LR


/*
 * int portDisableInt(void);
 */
    .global portDisableInt
    .type portDisableInt, %function
portDisableInt:
    MRS     R2, CPSR
    ORR     R1, R2, #CPU_FIQ_IRQ_MASK
    MSR     CPSR_c, R1
    AND     R0, R2, #CPU_FIQ_IRQ_MASK
    BX      LR

/*
 * void portRestoreInt( int state );
 */
    .global portRestoreInt
    .type portRestoreInt, %function
portRestoreInt:
    MRS     R2, CPSR
    BIC     R1, R2, #CPU_FIQ_IRQ_MASK
    ORR     R1, R1, R0
    MSR     CPSR_c, R1
    BX      LR

/*
 * int portIsInIRQ( void );
 */
    .global portIsInIRQ
    .type portIsInIRQ, %function
portIsInIRQ:
    MRS     R0, CPSR
    AND     R1, R0, #CPU_MODE_MASK
    CMP     R1, #CPU_MODE_IRQ
    MOVEQ   R0, #1
    MOVNE   R0, #0
    BX      LR

/*
 * int portIsInFIQ( void );
 */
    .global portIsInFIQ
    .type portIsInFIQ, %function
portIsInFIQ:
    MRS     R0, CPSR
    AND     R1, R0, #CPU_MODE_MASK
    CMP     R1, #CPU_MODE_FIQ
    MOVEQ   R0, #1
    MOVNE   R0, #0
    BX      LR


/*
 * void WFI( void );
 */
    .global WFI
    .type WFI, %function
WFI:
    MOV     R0, #0
    MCR     p15, 0, R0, c7, c0, 4
    BX      LR


/*
 * void arm9_enable_alignfault( void );
 */
    .global arm9_enable_alignfault
    .type arm9_enable_alignfault, %function
arm9_enable_alignfault:
    MRC     p15, 0, R0, c1, c0, 0
    ORR     R0, R0, #0x02
    MCR     p15, 0, R0, c1, c0, 0
    BX      LR

/*
 * void arm9_disable_alignfault( void );
 */
    .global arm9_disable_alignfault
    .type arm9_disable_alignfault, %function
arm9_disable_alignfault:
    MRC     p15, 0, R0, c1, c0, 0
    BIC     R0, R0, #0x02
    MCR     p15, 0, R0, c1, c0, 0
    BX      LR

/* EOF */
