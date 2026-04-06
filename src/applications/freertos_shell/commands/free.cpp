#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>


typedef struct {
    size_t used;
    size_t total;
} heap_stat_t;

extern "C" heap_stat_t newlib_heap_get_stat();

extern char _empty_ram;
extern char _stack_unused;


int command_free(__unused int argc, __unused const char *argv[]) {
    size_t xFreeHeapSizeCurrent = xPortGetFreeHeapSize();

    if (xFreeHeapSizeCurrent == 0) {
        vPortFree(pvPortMalloc(32));
        xFreeHeapSizeCurrent = xPortGetFreeHeapSize();
    }

    if (xFreeHeapSizeCurrent == 0) {
        printf("Not available\r\n");
        return 0;
    }


    printf("\r\n");

    HeapStats_t xHeapStats;
    vPortGetHeapStats(&xHeapStats);

    printf("          Heap size: %d\r\n", (&_stack_unused - &_empty_ram));
    printf("          available: %d\r\n", xHeapStats.xAvailableHeapSpaceInBytes);
    printf("            minimum: %d\r\n", xHeapStats.xMinimumEverFreeBytesRemaining);
    printf("      largest block: %d\r\n", xHeapStats.xSizeOfLargestFreeBlockInBytes);
    printf("     smallest block: %d\r\n", xHeapStats.xSizeOfSmallestFreeBlockInBytes);
    printf("   number of blocks: %d\r\n", xHeapStats.xNumberOfFreeBlocks);
    printf("  allocations count: %d\r\n", xHeapStats.xNumberOfSuccessfulAllocations);
    printf("        frees count: %d\r\n", xHeapStats.xNumberOfSuccessfulFrees);

    printf("\r\n");

    auto xNewlibStats = newlib_heap_get_stat();
    printf("        newlib heap\r\n");
    printf("               used: %d\r\n", xNewlibStats.used);
    printf("              total: %d\r\n", xNewlibStats.total);

    printf("\r\n");

    return 0;
}
