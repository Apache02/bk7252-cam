#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H


#define configCPU_CLOCK_HZ    ( ( unsigned long ) 120000000 )
#define configTICK_RATE_HZ                         1000

#define configUSE_PREEMPTION                        1
#define configUSE_TIME_SLICING                      1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION     0
#define configUSE_TICKLESS_IDLE                     1
#define configMAX_PRIORITIES                        5
#define configMINIMAL_STACK_SIZE                    128
#define configMAX_TASK_NAME_LEN                     16
#define configTICK_TYPE_WIDTH_IN_BITS               TICK_TYPE_WIDTH_64_BITS
#define configIDLE_SHOULD_YIELD                     1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES       3
#define configQUEUE_REGISTRY_SIZE                   10
#define configENABLE_BACKWARD_COMPATIBILITY         1
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS     0
#define configUSE_MINI_LIST_ITEM                    1
#define configSTACK_DEPTH_TYPE                      size_t
#define configMESSAGE_BUFFER_LENGTH_TYPE            size_t
#define configHEAP_CLEAR_MEMORY_ON_FREE             1
#define configSTATS_BUFFER_MAX_LENGTH               0xFFFF
#define configUSE_NEWLIB_REENTRANT                  0
#define configUSE_TIMERS                            1
#define configTIMER_TASK_PRIORITY                   ( configMAX_PRIORITIES - 1 )
#define configTIMER_TASK_STACK_DEPTH                configMINIMAL_STACK_SIZE
#define configTIMER_QUEUE_LENGTH                    10

#define configTIMER_SERVICE_TASK_NAME               "_timer"
#define configIDLE_TASK_NAME                        "_idle"

#define configUSE_EVENT_GROUPS                      0
#define configUSE_STREAM_BUFFERS                    1

#define configSUPPORT_STATIC_ALLOCATION             1
#define configSUPPORT_DYNAMIC_ALLOCATION            1
#define configTOTAL_HEAP_SIZE                       ((256 - 56) * 1024)
#define configAPPLICATION_ALLOCATED_HEAP            0
#define configSTACK_ALLOCATION_FROM_SEPARATE_HEAP   0
#define configENABLE_HEAP_PROTECTOR                 0

#define configKERNEL_INTERRUPT_PRIORITY             0
#define configMAX_SYSCALL_INTERRUPT_PRIORITY        0
#define configMAX_API_CALL_INTERRUPT_PRIORITY       0
#define configUSE_IDLE_HOOK                         0
#define configUSE_TICK_HOOK                         0
#define configUSE_MALLOC_FAILED_HOOK                0
#define configUSE_DAEMON_TASK_STARTUP_HOOK          0
#define configUSE_SB_COMPLETED_CALLBACK             0
#define configCHECK_FOR_STACK_OVERFLOW              2 // <--
#define configGENERATE_RUN_TIME_STATS               0
#define configUSE_TRACE_FACILITY                    1
#define configUSE_STATS_FORMATTING_FUNCTIONS        0
#define configUSE_CO_ROUTINES                       0
#define configMAX_CO_ROUTINE_PRIORITIES             1

#include <assert.h>
#define configASSERT(x)     assert(x)

#define configNUMBER_OF_CORES                       1
#define configKERNEL_PROVIDED_STATIC_MEMORY         1
#define configUSE_TASK_NOTIFICATIONS                1
#define configUSE_MUTEXES                           1
#define configUSE_RECURSIVE_MUTEXES                 1
#define configUSE_COUNTING_SEMAPHORES               1
#define configUSE_QUEUE_SETS                        0
#define configUSE_APPLICATION_TASK_TAG              0
#define configUSE_POSIX_ERRNO                       0
#define INCLUDE_vTaskPrioritySet                    1
#define INCLUDE_uxTaskPriorityGet                   1
#define INCLUDE_vTaskDelete                         1
#define INCLUDE_vTaskSuspend                        1
#define INCLUDE_xResumeFromISR                      1
#define INCLUDE_vTaskDelayUntil                     1
#define INCLUDE_vTaskDelay                          1
#define INCLUDE_xTaskGetSchedulerState              1
#define INCLUDE_xTaskGetCurrentTaskHandle           1
#define INCLUDE_uxTaskGetStackHighWaterMark         0
#define INCLUDE_xTaskGetIdleTaskHandle              0
#define INCLUDE_eTaskGetState                       0
#define INCLUDE_xEventGroupSetBitFromISR            1
#define INCLUDE_xTimerPendFunctionCall              0
#define INCLUDE_xTaskAbortDelay                     0
#define INCLUDE_xTaskGetHandle                      0
#define INCLUDE_xTaskResumeFromISR                  1
#define INCLUDE_xQueueGetMutexHolder                1

#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()    vConfigureTimerForRunTimeStats()
#define portGET_RUN_TIME_COUNTER_VALUE()            ulGetRunTimeCounterValue()

#define configRECORD_STACK_HIGH_ADDRESS             1

#endif 
