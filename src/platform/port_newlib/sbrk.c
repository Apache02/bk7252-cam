#include <errno.h>
#include "heap.h"

extern char _empty_ram_begin;
extern char _empty_ram_end;

static char *heap_end = &_empty_ram_begin;

__attribute__((weak)) void *_sbrk(ptrdiff_t incr) {
    char *prev_heap_end = heap_end;

    if (heap_end + incr > &_empty_ram_end) {
        errno = ENOMEM;
        return (void *)-1;
    }

    heap_end += incr;
    return (void *)prev_heap_end;
}

__attribute__((weak)) heap_stat_t newlib_heap_get_stat() {
    return (heap_stat_t){
        .used  = (size_t)(heap_end - &_empty_ram_begin),
        .total = (size_t)(&_empty_ram_end - &_empty_ram_begin),
    };
}
