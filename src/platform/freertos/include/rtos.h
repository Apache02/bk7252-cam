#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int    OSStatus;
typedef int    BOOL;
typedef void * beken_thread_t;
typedef void * beken_semaphore_t;
typedef void * beken_mutex_t;
typedef void * beken_queue_t;

typedef void (*beken_thread_function_t)(void *arg);

#define kNoErr       0
#define kGeneralErr  (-1)
#define kTimeoutErr  (-6722)

#define BEKEN_WAIT_FOREVER  (0xFFFFFFFFUL)
#define BEKEN_NO_WAIT       (0UL)

OSStatus rtos_create_thread(beken_thread_t *thread, uint8_t priority, const char *name,
                             beken_thread_function_t function, uint32_t stack_size, void *arg);
OSStatus rtos_delete_thread(beken_thread_t *thread);
OSStatus rtos_thread_set_priority(beken_thread_t *thread, int priority);
BOOL     rtos_is_current_thread(beken_thread_t *thread);
OSStatus rtos_delay_milliseconds(uint32_t num_ms);

OSStatus rtos_init_semaphore(beken_semaphore_t *semaphore, int maxCount);
OSStatus rtos_get_semaphore(beken_semaphore_t *semaphore, uint32_t timeout_ms);
int      rtos_set_semaphore(beken_semaphore_t *semaphore);
OSStatus rtos_deinit_semaphore(beken_semaphore_t *semaphore);

OSStatus rtos_init_mutex(beken_mutex_t *mutex);
OSStatus rtos_lock_mutex(beken_mutex_t *mutex);
OSStatus rtos_unlock_mutex(beken_mutex_t *mutex);
OSStatus rtos_deinit_mutex(beken_mutex_t *mutex);

OSStatus rtos_init_queue(beken_queue_t *queue, const char *name,
                          uint32_t message_size, uint32_t number_of_messages);
OSStatus rtos_push_to_queue(beken_queue_t *queue, void *message, uint32_t timeout_ms);
OSStatus rtos_pop_from_queue(beken_queue_t *queue, void *message, uint32_t timeout_ms);
OSStatus rtos_deinit_queue(beken_queue_t *queue);

#ifdef __cplusplus
}
#endif
