#include <errno.h>
#include "heap.h"

extern char _empty_ram;
extern char _stack_unused;

static char *heap_end = &_empty_ram;

__attribute__((weak)) void *_sbrk(ptrdiff_t incr) {
    char *prev_heap_end = heap_end;

    if (heap_end + incr > &_stack_unused) {
        errno = ENOMEM;
        return (void *)-1;
    }

    heap_end += incr;
    return (void *)prev_heap_end;
}

__attribute__((weak)) heap_stat_t newlib_heap_get_stat() {
    return (heap_stat_t){
        .used  = (size_t)(heap_end - &_empty_ram),
        .total = (size_t)(&_stack_unused - &_empty_ram),
    };
}
