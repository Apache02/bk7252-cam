#include <FreeRTOS.h>
#include <errno.h>

extern char _empty_ram_begin;
extern char _empty_ram_end;

void heap_init() {
    vPortHeapResetState();

    HeapRegion_t regions[] = {
        {
            (uint8_t *)&_empty_ram_begin, (size_t)(&_empty_ram_end - &_empty_ram_begin)
        },
        {NULL, 0}
    };
    vPortDefineHeapRegions(regions);
}
