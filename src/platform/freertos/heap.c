#include <heap.h>
#include <string.h>
#include <FreeRTOS.h>

__attribute__((weak)) void *pvPortRealloc(void *ptr, size_t size)
{
    if (!ptr) {
        return size ? pvPortMalloc(size) : NULL;
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
