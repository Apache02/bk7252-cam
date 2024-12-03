#include <cstdlib>
#include "FreeRTOS.h"

void *operator new(size_t size) noexcept {
    return pvPortMalloc(size);
}

void operator delete(void *ptr) noexcept {
    vPortFree(ptr);
}

void *operator new[](size_t size) noexcept {
    return pvPortMalloc(size);
}

void operator delete[](void *ptr) noexcept {
    vPortFree(ptr);
}

