#include <string.h>
#include <errno.h>
#include <stddef.h>
#include "heap.h"
#include "FreeRTOS.h"
#include "task.h"

#ifndef NEWLIB_HEAP_SIZE
#define NEWLIB_HEAP_SIZE (4 * 1024)
#endif

static char  newlib_heap[NEWLIB_HEAP_SIZE];
static char *newlib_heap_end = newlib_heap;

void *_sbrk(ptrdiff_t incr) {
    vTaskSuspendAll();
    char *prev = newlib_heap_end;
    if (newlib_heap_end + incr > newlib_heap + NEWLIB_HEAP_SIZE) {
        xTaskResumeAll();
        errno = ENOMEM;
        return (void *)-1;
    }
    newlib_heap_end += incr;
    xTaskResumeAll();
    return (void *)prev;
}

heap_stat_t newlib_heap_get_stat() {
    return (heap_stat_t){
        .used  = (size_t)(newlib_heap_end - newlib_heap),
        .total = (size_t)(NEWLIB_HEAP_SIZE),
    };
}

// allocator

void *malloc(size_t size) { return pvPortMalloc(size); }

void free(void *ptr) { vPortFree(ptr); }

void *calloc(size_t n, size_t size) { return pvPortCalloc(n, size); }

extern void *pvPortRealloc(void *ptr, size_t size);

void *realloc(void *ptr, size_t size) {
    return pvPortRealloc(ptr, size);
}
