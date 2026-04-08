#include "tlsf_global.h"
#include "heap.h"


extern char _empty_ram;
extern char _stack_unused;

tlsf_t g_pvTLSF;
pool_t g_pvTLSFPool;
size_t g_xMinimumEverFreeBytesRemaining;


void heap_init() {
    void *block = &_empty_ram;
    size_t size = (size_t) (&_stack_unused - &_empty_ram);
    size_t pool_size = size - tlsf_size();

    g_pvTLSF = tlsf_create(block);
    g_pvTLSFPool = tlsf_add_pool(g_pvTLSF, (char *) block + tlsf_size(), pool_size);

    g_xMinimumEverFreeBytesRemaining = pool_size;
}

