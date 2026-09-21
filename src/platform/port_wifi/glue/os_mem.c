// The vendor binaries route allocation and bulk memory operations through this
// os_* layer. Only the entry points they import are defined.

#include <stddef.h>
#include <string.h>

#include <FreeRTOS.h>

/* ---- heap ---- */

// imported: libip(ke_msg.o, ke_timer.o, me_task.o, rwnx.o, rxl_cntrl.o)
void *os_malloc(size_t size) { return pvPortMalloc(size); }

// imported: libip(hal_machw.o)
void *os_zalloc(size_t size) {
    void *ptr = pvPortMalloc(size);
    if (ptr) memset(ptr, 0, size);
    return ptr;
}

// imported: libip(hal_machw.o, ke_event.o, ke_msg.o, ke_timer.o, me.o, me_task.o, mm_bcn.o, rwnx.o, rxl_cntrl.o)
void os_free(void *ptr) { vPortFree(ptr); }

/* ---- memory ops ---- */

// imported: libip(hal_dma.o, hal_machw.o, me.o, me_task.o, rxu_cntrl.o, scan.o, sm_task.o, txl_frame.o)
void *os_memcpy(void *dst, const void *src, unsigned int n) { return memcpy(dst, src, n); }

// imported: libip(rxl_cntrl.o, scan.o, scanu.o, sta_mgmt.o, tx_swdesc.o)
void *os_memset(void *s, int c, unsigned int n) { return memset(s, c, n); }

// imported: libip(rxu_cntrl.o, sta_mgmt.o)
int os_memcmp(const void *s1, const void *s2, unsigned int n) { return memcmp(s1, s2, n); }

