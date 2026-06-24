#include <FreeRTOS.h>
#include <errno.h>

extern char _empty_ram;
extern char _stack_unused;

void heap_init() {
    vPortHeapResetState();

    HeapRegion_t regions[] = {
        {
            (uint8_t *)&_empty_ram, (size_t)(&_stack_unused - &_empty_ram)
        },
        {NULL, 0}
    };
    vPortDefineHeapRegions(regions);
}
