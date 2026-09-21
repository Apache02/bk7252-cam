#pragma once

// ip/lmac/src/rx/rxl_cntrl.h — the archive's receive path, as far as the host
// has to reach into it.
//
// Both calls are entry points, not queries: one from the interrupt handler, one
// from the task that serialises everything into the archive.

#ifdef __cplusplus
extern "C" {
#endif

// The RX DMA's own interrupt service routine, which the host's MAC interrupt
// handler calls straight through.
void rxl_dma_int_handler(void);

// Hands a received frame back to the archive. The argument is never read, so
// this only says "a frame is ready", from the WiFi core task rather than from
// the interrupt that noticed.
void rxl_cntrl_evt(int dummy);

#ifdef __cplusplus
}
#endif
