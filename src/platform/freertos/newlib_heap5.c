#include <FreeRTOS.h>
#include <string.h>
#include <errno.h>


// Heap5

extern char _empty_ram;
extern char _stack_unused;

void heap_init() {
    vPortHeapResetState();

    HeapRegion_t regions[] = {
        {(uint8_t *) &_empty_ram, (size_t) (&_stack_unused - &_empty_ram)},
        {NULL, 0}
    };
    vPortDefineHeapRegions(regions);
}


// newlib

#ifndef NEWLIB_HEAP_SIZE
#define NEWLIB_HEAP_SIZE        (4*1024)
#endif

static char newlib_heap[NEWLIB_HEAP_SIZE];
static char *newlib_heap_end = newlib_heap;

void *_sbrk(ptrdiff_t incr) {
    char *prev = newlib_heap_end;
    if (newlib_heap_end + incr > newlib_heap + NEWLIB_HEAP_SIZE) {
        errno = ENOMEM;
        return (void *) -1;
    }
    newlib_heap_end += incr;
    return (void *) prev;
}

typedef struct {
    size_t used;
    size_t total;
} heap_stat_t;

heap_stat_t newlib_heap_get_stat() {
    return (heap_stat_t){
        .used = (size_t) (newlib_heap_end - newlib_heap),
        .total = (size_t) (NEWLIB_HEAP_SIZE),
    };
}

// allocator

void *malloc(size_t size) {
    return pvPortMalloc(size);
}

void free(void *ptr) {
    vPortFree(ptr);
}

void *calloc(size_t n, size_t size) {
    void *p = pvPortMalloc(n * size);
    if (p) {
        memset(p, 0, n * size);
    }
    return p;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) {
        return pvPortMalloc(size);
    }
    if (!size) {
        vPortFree(ptr);
        return NULL;
    }
    void *p = pvPortMalloc(size);
    if (p) {
        memcpy(p, ptr, size);
        vPortFree(ptr);
    }
    return p;
}
