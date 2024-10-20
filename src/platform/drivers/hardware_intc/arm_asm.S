#include "arm.h"


.text
    .align  4

;/*
; * void portENABLE_IRQ( void );
; */
    .globl portENABLE_IRQ
    .type portENABLE_IRQ, %function
portENABLE_IRQ:
    MRS     R0, CPSR
    BIC     R1, R0, #CPU_IRQ_MASK
    MSR     CPSR_c, R1
    BX      LR

;/*
; * int portDISABLE_IRQ( void );
; */
    .globl portDISABLE_IRQ
    .type portDISABLE_IRQ, %function
portDISABLE_IRQ:
    MRS     R2, CPSR
    ORR     R1, R2, #CPU_IRQ_MASK
    MSR     CPSR_c, R1
    AND     R0, R2, #CPU_IRQ_MASK
    BX      LR


;/*
; * void portENABLE_FIQ( void );
; */
    .globl portENABLE_FIQ
    .type portENABLE_FIQ, %function
portENABLE_FIQ:
    MRS     R0, CPSR
    BIC     R1, R0, #CPU_FIQ_MASK
    MSR     CPSR_c, R1
    BX      LR

;/*
; * int portDISABLE_FIQ( void );
; */
    .globl portDISABLE_FIQ
    .type portDISABLE_FIQ, %function
portDISABLE_FIQ:
    MRS     R2, CPSR
    ORR     R1, R2, #CPU_FIQ_MASK
    MSR     CPSR_c, R1
    AND     R0, R2, #CPU_FIQ_MASK
    BX      LR


;/*
; * int portDisableInt(void);
; */
    .globl portDisableInt
    .type portDisableInt, %function
portDisableInt:
    MRS     R2, CPSR
    ORR     R1, R2, #CPU_FIQ_IRQ_MASK
    MSR     CPSR_c, R1
    AND     R0, R2, #CPU_FIQ_IRQ_MASK
    BX      LR

;/*
; * void portRestoreInt( int state );
; */
    .globl portRestoreInt
    .type portRestoreInt, %function
portRestoreInt:
    MRS     R2, CPSR
    BIC     R1, R2, #CPU_FIQ_IRQ_MASK
    ORR     R1, R1, R0
    MSR     CPSR_c, R1
    BX      LR

