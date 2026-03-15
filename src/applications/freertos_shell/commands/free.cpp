#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>

extern char _empty_ram;
extern char _stack_unused;

extern "C" void *_sbrk(ptrdiff_t incr);

int command_free(__unused int argc, __unused const char *argv[]) {
    size_t xFreeHeapSizeCurrent = xPortGetFreeHeapSize();

    if (xFreeHeapSizeCurrent == 0) {
        vPortFree(pvPortMalloc(32));
        xFreeHeapSizeCurrent = xPortGetFreeHeapSize();
    }

    if (xFreeHeapSizeCurrent == 0) {
        printf("Not available\r\n");
    }

    size_t xFreeHeapSizeMinimal = xPortGetMinimumEverFreeHeapSize();

    printf("Free heap current (bytes): %d of %d\r\n", xFreeHeapSizeCurrent, configTOTAL_HEAP_SIZE);
    printf("          minimum (bytes): %d\r\n", xFreeHeapSizeMinimal);

    size_t total = &_stack_unused - &_empty_ram;
    size_t used = (char *) _sbrk(0) - &_empty_ram;
    printf(" newlib heap used (bytes): %d of %d\r\n", used, total);

    return 0;
}
