#ifndef PORTMACRO_H
#define PORTMACRO_H


#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


/* Type definitions. */
#define portCHAR            char
#define portFLOAT           float
#define portDOUBLE          double
#define portLONG            long
#define portSHORT           short
#define portSTACK_TYPE      uint32_t
#define portBASE_TYPE       long

typedef portSTACK_TYPE      StackType_t;
typedef long                BaseType_t;
typedef unsigned long       UBaseType_t;
typedef uint32_t            TickType_t;
#define portMAX_DELAY       ( TickType_t ) 0xffffffffUL

/* Constants required to setup the initial stack. */
#define portTHUMB_MODE_BIT              ( ( StackType_t ) 0x20 )
#define portINITIAL_SPSR                ( ( StackType_t ) 0x1F ) /* System mode, ARM mode, interrupts enabled. */
#define portINSTRUCTION_SIZE            ( ( StackType_t ) 4 )

/*-----------------------------------------------------------*/

/* Architecture specifics. */
#define portSTACK_GROWTH    ( -1 )
#define portTICK_PERIOD_MS  ( ( TickType_t ) 1000 / configTICK_RATE_HZ )
#define portBYTE_ALIGNMENT  8
#define portDONT_DISCARD    __attribute__( ( used ) )
//#define portYIELD()         __asm ( "SWI 0" )
extern void vPortYield( void );
#define portYIELD()         vPortYield()

/*-----------------------------------------------------------*/

void vPortEnableInterrupts();
void vPortDisableInterrupts();

#define portENABLE_INTERRUPTS()     vPortEnableInterrupts()
#define portDISABLE_INTERRUPTS()    vPortDisableInterrupts()

/*-----------------------------------------------------------
* Critical section control
*
* The code generated by the Keil compiler does not maintain separate
* stack and frame pointers. The portENTER_CRITICAL macro cannot therefore
* use the stack as per other ports.  Instead a variable is used to keep
* track of the critical section nesting.  This necessitates the use of a
* function in place of the macro.
*----------------------------------------------------------*/

extern void vPortEnterCritical( void );
extern void vPortExitCritical( void );

#define portENTER_CRITICAL()    vPortEnterCritical();
#define portEXIT_CRITICAL()     vPortExitCritical();

/*-----------------------------------------------------------*/

/* Task function macros as described on the FreeRTOS.org WEB site. */
#define portTASK_FUNCTION_PROTO(vFunction, pvParameters)    void vFunction( void * pvParameters )
#define portTASK_FUNCTION(vFunction, pvParameters)          void vFunction( void * pvParameters )


#ifdef __cplusplus
}
#endif

#endif /* PORTMACRO_H */