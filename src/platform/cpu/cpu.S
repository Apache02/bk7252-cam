#include "platform/arm.h"

.syntax unified

.text
.arm

//      int portENABLE_IRQ( void );
    .global portENABLE_IRQ
    .type portENABLE_IRQ, %function
portENABLE_IRQ:
    mrs     r0, cpsr
    bic     r1, r0, #CPU_IRQ_MASK
    msr     cpsr_c, r1
    bx      lr

//      int portDISABLE_IRQ( void );
    .global portDISABLE_IRQ
    .type portDISABLE_IRQ, %function
portDISABLE_IRQ:
    mrs     r2, cpsr
    orr     r1, r2, #CPU_IRQ_MASK
    msr     cpsr_c, r1
    and     r0, r2, #CPU_IRQ_MASK
    bx      lr

//      int portENABLE_FIQ( void );
    .global portENABLE_FIQ
    .type portENABLE_FIQ, %function
portENABLE_FIQ:
    mrs     r0, cpsr
    bic     r1, r0, #CPU_FIQ_MASK
    msr     cpsr_c, r1
    bx      lr

//      int portDISABLE_FIQ( void );
    .global portDISABLE_FIQ
    .type portDISABLE_FIQ, %function
portDISABLE_FIQ:
    mrs     r2, cpsr
    orr     r1, r2, #CPU_FIQ_MASK
    msr     cpsr_c, r1
    and     r0, r2, #CPU_FIQ_MASK
    bx      lr

//      int portDisableInt( void );
    .global portDisableInt
    .type portDisableInt, %function
portDisableInt:
    mrs     r2, cpsr
    orr     r1, r2, #CPU_FIQ_IRQ_MASK
    msr     cpsr_c, r1
    and     r0, r2, #CPU_FIQ_IRQ_MASK
    bx      lr

//      void portRestoreInt( int state );
    .global portRestoreInt
    .type portRestoreInt, %function
portRestoreInt:
    mrs     r2, cpsr
    bic     r1, r2, #CPU_FIQ_IRQ_MASK
    orr     r1, r1, r0
    msr     cpsr_c, r1
    bx      lr

//      int portIsInIRQ( void );
    .global portIsInIRQ
    .type portIsInIRQ, %function
portIsInIRQ:
    mrs     r0, cpsr
    and     r1, r0, #CPU_MODE_MASK
    cmp     r1, #CPU_MODE_IRQ
    moveq   r0, #1
    movne   r0, #0
    bx      lr

//      int portIsInFIQ( void );
    .global portIsInFIQ
    .type portIsInFIQ, %function
portIsInFIQ:
    mrs     r0, cpsr
    and     r1, r0, #CPU_MODE_MASK
    cmp     r1, #CPU_MODE_FIQ
    moveq   r0, #1
    movne   r0, #0
    bx      lr

//     void WFI( void );
    .global WFI
    .type WFI, %function
WFI:
    mov     r0, #0
    mcr     p15, 0, r0, c7, c0, 4
    bx      lr

//     void arm9_enable_alignfault( void );
    .global arm9_enable_alignfault
    .type arm9_enable_alignfault, %function
arm9_enable_alignfault:
    mrc     p15, 0, r0, c1, c0, 0
    orr     r0, r0, #0x02
    mcr     p15, 0, r0, c1, c0, 0
    bx      lr

//     void arm9_disable_alignfault( void );
    .global arm9_disable_alignfault
    .type arm9_disable_alignfault, %function
arm9_disable_alignfault:
    mrc     p15, 0, r0, c1, c0, 0
    bic     r0, r0, #0x02
    mcr     p15, 0, r0, c1, c0, 0
    bx      lr
