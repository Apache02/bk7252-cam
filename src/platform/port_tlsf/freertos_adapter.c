#include <string.h>
#include "tlsf_global.h"
#include "FreeRTOS.h"
#include "task.h"


static size_t xNumberOfSuccessfulAllocations = 0;
static size_t xNumberOfSuccessfulFrees = 0;

static void pool_walker(void *ptr, size_t size, int used, void *user);


void *pvPortMalloc(size_t xWantedSize) {
    if (xWantedSize == 0) {
        return NULL;
    }

    vTaskSuspendAll();

    void *pv = tlsf_malloc(g_pvTLSF, xWantedSize);

    if (pv) {
        // update statistics
        xNumberOfSuccessfulAllocations++;

        HeapStats_t s = {0};
        tlsf_walk_pool(g_pvTLSFPool, pool_walker, &s);
        if (g_xMinimumEverFreeBytesRemaining > s.xAvailableHeapSpaceInBytes) {
            g_xMinimumEverFreeBytesRemaining = s.xAvailableHeapSpaceInBytes;
        }
    }

    xTaskResumeAll();

#if ( configUSE_MALLOC_FAILED_HOOK == 1 )
    if (pv == NULL) {
        vApplicationMallocFailedHook();
    }
#endif

    return pv;
}

void *pvPortCalloc(size_t xNum, size_t xSize) {
    size_t total = xNum * xSize;
    void *p = pvPortMalloc(total);
    if (p) {
        memset(p, 0, total);
    }

    return p;
}

void vPortFree(void *pv) {
    if (!pv) return;

    vTaskSuspendAll();
    tlsf_free(g_pvTLSF, pv);
    xNumberOfSuccessfulFrees++;
    xTaskResumeAll();
}

size_t xPortGetFreeHeapSize(void) {
    HeapStats_t s = {0};

    vTaskSuspendAll();
    tlsf_walk_pool(g_pvTLSFPool, pool_walker, &s);
    xTaskResumeAll();

    return s.xAvailableHeapSpaceInBytes;
}

size_t xPortGetMinimumEverFreeHeapSize(void) {
    return g_xMinimumEverFreeBytesRemaining;
}

static void pool_walker(__unused void *ptr, size_t size, int used, void *user) {
    HeapStats_t *s = (HeapStats_t *) user;

    if (!used) {
        s->xAvailableHeapSpaceInBytes += size;
        s->xNumberOfFreeBlocks++;
        if (size > s->xSizeOfLargestFreeBlockInBytes) {
            s->xSizeOfLargestFreeBlockInBytes = size;
        }
        if (size < s->xSizeOfSmallestFreeBlockInBytes || s->xSizeOfSmallestFreeBlockInBytes == 0) {
            s->xSizeOfSmallestFreeBlockInBytes = size;
        }
    }
}

void vPortGetHeapStats(HeapStats_t *pxHeapStats) {
    pxHeapStats->xAvailableHeapSpaceInBytes = 0;
    pxHeapStats->xSizeOfLargestFreeBlockInBytes = 0;
    pxHeapStats->xSizeOfSmallestFreeBlockInBytes = 0;
    pxHeapStats->xNumberOfFreeBlocks = 0;

    vTaskSuspendAll();
    tlsf_walk_pool(g_pvTLSFPool, pool_walker, pxHeapStats);
    xTaskResumeAll();

    pxHeapStats->xMinimumEverFreeBytesRemaining = g_xMinimumEverFreeBytesRemaining;
    pxHeapStats->xNumberOfSuccessfulAllocations = xNumberOfSuccessfulAllocations;
    pxHeapStats->xNumberOfSuccessfulFrees = xNumberOfSuccessfulFrees;
}
