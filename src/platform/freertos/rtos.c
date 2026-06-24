#include <rtos.h>

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

/* Beken priority 0 = highest; FreeRTOS priority (configMAX_PRIORITIES-1) = highest.
   BK 0 → native (configMAX_PRIORITIES-1), BK N → native (configMAX_PRIORITIES-1-N). */
#define BK_PRI(p) ((UBaseType_t)(configMAX_PRIORITIES - 1u - (unsigned)(p)))

extern int portIsInIRQ(void);
extern int portIsInFIQ(void);

static inline int in_interrupt(void) { return portIsInIRQ() || portIsInFIQ(); }

static TickType_t to_ticks(uint32_t ms) { return (ms == BEKEN_WAIT_FOREVER) ? portMAX_DELAY : pdMS_TO_TICKS(ms); }

/* ---- threads ---- */

OSStatus rtos_create_thread(beken_thread_t *thread, uint8_t priority, const char *name,
                            beken_thread_function_t function, uint32_t stack_size, void *arg) {
    if (xTaskCreate((TaskFunction_t)function, name, (configSTACK_DEPTH_TYPE)(stack_size / sizeof(StackType_t)), arg,
                    BK_PRI(priority), (TaskHandle_t *)thread) == pdPASS)
        return kNoErr;
    return kGeneralErr;
}

OSStatus rtos_delete_thread(beken_thread_t *thread) {
    vTaskDelete(thread ? (TaskHandle_t)*thread : NULL);
    return kNoErr;
}

OSStatus rtos_thread_set_priority(beken_thread_t *thread, int priority) {
    vTaskPrioritySet((TaskHandle_t)*thread, BK_PRI(priority));
    return kNoErr;
}

BOOL rtos_is_current_thread(beken_thread_t *thread) {
    return (*thread == xTaskGetCurrentTaskHandle()) ? pdTRUE : pdFALSE;
}

OSStatus rtos_delay_milliseconds(uint32_t num_ms) {
    vTaskDelay(pdMS_TO_TICKS(num_ms));
    return kNoErr;
}

/* ---- semaphores ---- */

OSStatus rtos_init_semaphore(beken_semaphore_t *semaphore, int maxCount) {
    *semaphore = xSemaphoreCreateCounting((UBaseType_t)maxCount, 0);
    return (*semaphore != NULL) ? kNoErr : kGeneralErr;
}

OSStatus rtos_get_semaphore(beken_semaphore_t *semaphore, uint32_t timeout_ms) {
    return (xSemaphoreTake(*semaphore, to_ticks(timeout_ms)) == pdTRUE) ? kNoErr : kTimeoutErr;
}

int rtos_set_semaphore(beken_semaphore_t *semaphore) {
    if (in_interrupt()) {
        BaseType_t woken = pdFALSE;
        xSemaphoreGiveFromISR(*semaphore, &woken);
        portYIELD_FROM_ISR(woken);
        return kNoErr;
    }
    return (xSemaphoreGive(*semaphore) == pdTRUE) ? kNoErr : kGeneralErr;
}

OSStatus rtos_deinit_semaphore(beken_semaphore_t *semaphore) {
    if (semaphore && *semaphore) {
        vSemaphoreDelete(*semaphore);
        *semaphore = NULL;
    }
    return kNoErr;
}

/* ---- mutexes ---- */

OSStatus rtos_init_mutex(beken_mutex_t *mutex) {
    *mutex = xSemaphoreCreateMutex();
    return (*mutex != NULL) ? kNoErr : kGeneralErr;
}

OSStatus rtos_lock_mutex(beken_mutex_t *mutex) {
    return (xSemaphoreTake(*mutex, portMAX_DELAY) == pdTRUE) ? kNoErr : kGeneralErr;
}

OSStatus rtos_unlock_mutex(beken_mutex_t *mutex) { return (xSemaphoreGive(*mutex) == pdTRUE) ? kNoErr : kGeneralErr; }

OSStatus rtos_deinit_mutex(beken_mutex_t *mutex) {
    vSemaphoreDelete(*mutex);
    *mutex = NULL;
    return kNoErr;
}

/* ---- queues ---- */

OSStatus rtos_init_queue(beken_queue_t *queue, const char *name, uint32_t message_size, uint32_t number_of_messages) {
    (void)name;
    *queue = xQueueCreate(number_of_messages, message_size);
    return (*queue != NULL) ? kNoErr : kGeneralErr;
}

OSStatus rtos_push_to_queue(beken_queue_t *queue, void *message, uint32_t timeout_ms) {
    if (in_interrupt()) {
        BaseType_t woken = pdFALSE;
        BaseType_t r     = xQueueSendToBackFromISR(*queue, message, &woken);
        portYIELD_FROM_ISR(woken);
        return (r == pdTRUE) ? kNoErr : kGeneralErr;
    }
    return (xQueueSendToBack(*queue, message, to_ticks(timeout_ms)) == pdTRUE) ? kNoErr : kGeneralErr;
}

OSStatus rtos_pop_from_queue(beken_queue_t *queue, void *message, uint32_t timeout_ms) {
    return (xQueueReceive(*queue, message, to_ticks(timeout_ms)) == pdTRUE) ? kNoErr : kTimeoutErr;
}

OSStatus rtos_deinit_queue(beken_queue_t *queue) {
    vQueueDelete(*queue);
    *queue = NULL;
    return kNoErr;
}
