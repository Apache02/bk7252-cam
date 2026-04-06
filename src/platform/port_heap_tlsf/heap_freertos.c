#include <stddef.h>
#include <string.h>
#include "tlsf.h"
#include "heap.h"
#include "FreeRTOS.h"
#include "task.h"

extern char _empty_ram;
extern char _stack_unused;

static tlsf_t s_tlsf;
static size_t s_min_free;
static pool_t s_pool;


static inline void heap_lock() {
    vTaskSuspendAll();
}

static inline void heap_unlock() {
    xTaskResumeAll();
}

void heap_init(void) {
    void *block = &_empty_ram;
    size_t size = (size_t) (&_stack_unused - &_empty_ram);
    size_t pool_size = size - tlsf_size();

    s_tlsf = tlsf_create(block);
    s_pool = tlsf_add_pool(s_tlsf, (char *) block + tlsf_size(), pool_size);

    s_min_free = pool_size;
}

static void pool_walker(__unused void *ptr, size_t size, int used, void *user) {
    heap_statistics_t *s = (heap_statistics_t *) user;

    if (used) {
        s->used_bytes += size;
        s->used_blocks++;
    } else {
        s->free_bytes += size;
        s->free_blocks++;
        if (size > s->largest_free) {
            s->largest_free = size;
        }
    }
}

heap_statistics_t heap_statistics() {
    heap_statistics_t s = {0};
    heap_lock();
    tlsf_walk_pool(s_pool, pool_walker, &s);
    heap_unlock();
    return s;
}

static inline void refresh_min_free() {
    heap_statistics_t s = heap_statistics();
    if (s.free_bytes < s_min_free) {
        s_min_free = s.free_bytes;
    }
}


/* -------------------------------------------------------------------------
 * malloc/free - newlib and C++
 * ---------------------------------------------------------------------- */
void *malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    heap_lock();
    void *p = tlsf_malloc(s_tlsf, size);
    if (p) refresh_min_free();
    heap_unlock();

#if ( configUSE_MALLOC_FAILED_HOOK == 1 )
    if (p == NULL) {
        vApplicationMallocFailedHook();
    }
#endif

    return p;
}

void free(void *p) {
    if (!p) return;

    heap_lock();
    tlsf_free(s_tlsf, p);
    heap_unlock();
}

void *calloc(size_t n, size_t size) {
    size_t total = n * size;
    void *p = malloc(total);
    if (p) {
        memset(p, 0, total);
    }

    return p;
}

void *realloc(void *p, size_t size) {
    if (!p) {
        return malloc(size);
    }
    if (size == 0) {
        free(p);
        return NULL;
    }

    heap_lock();
    void *r = tlsf_realloc(s_tlsf, p, size);
    heap_unlock();

    return r;
}


/* -------------------------------------------------------------------------
 * FreeRTOS allocator interface
 * ---------------------------------------------------------------------- */
void *pvPortMalloc(size_t size) {
    return malloc(size);
}

void vPortFree(void *p) {
    free(p);
}

size_t xPortGetFreeHeapSize(void) {
    heap_statistics_t s = heap_statistics();
    return s.free_bytes;
}

size_t xPortGetMinimumEverFreeHeapSize(void) {
    return s_min_free;
}

void vPortHeapResetState() {
    size_t size = (size_t) (&_stack_unused - &_empty_ram);
    size_t pool_size = size - tlsf_size();
    s_min_free = pool_size;
}

// TODO:
// void vPortGetHeapStats( HeapStats_t * pxHeapStats )
