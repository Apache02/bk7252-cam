#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "hardware/intc.h"
#include "soc/icu.h"

#include "libip/ke.h"
#include "libip/rx.h"
#include "libip/tx.h"

#include "wifi/platform_glue.h"

// #define DEBUG_NAME "platform_glue"
#include "debug.h"

// ---- bk_printf / os_null_printf ---------------------------------------------
// imported: libip(38 objects)

void bk_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

// imported: libip(me_task.o, ps.o, rwnx.o, rxl_cntrl.o, rxu_cntrl.o)
int os_null_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vprintf(fmt, ap);
    va_end(ap);

    return ret;
}

// ---- sctrl_modem_core_reset ------------------------------------------------
// imported: libip(hal_machw.o) — satisfied by hardware_sctrl
// (src/platform/drivers/hardware_sctrl/sctrl.c), not by this file.

// ---- intc_service_register -------------------------------------------------
// imported: libip(hal_machw.o, rxl_cntrl.o, txl_cntrl.o)

void intc_service_register(uint8_t int_num, __unused uint8_t int_pri, void (*isr)(void)) {
    // SDK int_num space (driver/include/intc_pub.h) is bit-for-bit our IRQ_SOURCE_*/
    // FIQ_SOURCE_* (1 << int_num; bit 0 = UART1 … bit 15 = GDMA … bit 16 = MODEM …
    // bit 22 = MAC_GENERAL …). Registration is unified under intc_register_irq_handler
    // regardless of which half the bit falls in.
    // LOG_I("%s(int_num=%u, int_pri=%u, isr=%p)", __func__, int_num, int_pri, (void *)isr);

    if (int_num >= 32) {
        LOG_W("%s(int_num=%u): out of range", __func__, int_num);
        return;
    }

    uint32_t source = 1u << int_num;
    intc_register_irq_handler(source, isr);
    intc_enable_irq_source(source);
}

// ---- intc_spurious ---------------------------------------------------------
// imported: libip(rxl_cntrl.o)
// Catch-all for unexpected IRQ numbers — should never fire in a correct system.

void intc_spurious(void) { __builtin_trap(); }

// ---- dma_push --------------------------------------------------------------
// imported: libip(rxl_cntrl.o, rxu_cntrl.o, txl_buffer.o, txl_cfm.o)
// WiFi MAC / IPC DMA (RivieraWaves), not the SoC GDMA — ported from the SDK's
// driver/dma/dma.c. It touches no DMA register: it walks the descriptor chain
// with a CPU copy, then signals the rwnx layer that the transfer completed.

struct dma_desc {
    uint32_t src;
    uint32_t dest;
    uint16_t length;
    uint16_t ctrl;
    uint32_t next;
};

// Channel indexes, order fixed by the archive's ABI (dma.h: IPC_DMA_CHANNEL_*).
enum {
    IPC_DMA_CHANNEL_CTRL_RX,
    IPC_DMA_CHANNEL_DATA_RX,
    IPC_DMA_CHANNEL_CTRL_TX,
    IPC_DMA_CHANNEL_DATA_TX,
};

uint32_t dma_push(struct dma_desc *first, struct dma_desc *last, uint32_t channel_idx) {
    uint32_t         push_len = 0;
    struct dma_desc *desc     = first;
    int              go_on    = 1;

    while (go_on && desc) {
        // TODO: use hardware_gdma (gdma_reserve_channel/gdma_run) instead of a
        // blocking CPU memcpy once the WiFi datapath needs the throughput —
        // the original SDK does the same CPU copy here unless CFG_GENERAL_DMA
        // is enabled, so this matches upstream behavior for now.
        memcpy((void *)(uintptr_t)desc->dest, (const void *)(uintptr_t)desc->src, desc->length);
        push_len += desc->length;

        if (desc == last) {
            go_on = 0;
        }
        desc = (struct dma_desc *)(uintptr_t)desc->next;
    }

    uint32_t event = 0;

    switch (channel_idx) {
        case IPC_DMA_CHANNEL_CTRL_RX:
        case IPC_DMA_CHANNEL_CTRL_TX:
            break;
        case IPC_DMA_CHANNEL_DATA_RX:
            rxl_dma_int_handler();
            break;
        case IPC_DMA_CHANNEL_DATA_TX:
            // Newer vendor trees pack the access category into the high byte of
            // channel_idx; this archive never does, so the queue is unknown and
            // all four AC payload bits are raised. Safe because
            // txl_payload_handle() clears its own bit on an empty queue.
            for (uint32_t ac = 0; ac < NX_TXQ_CNT; ac++) {
                event |= ke_get_ac_payload_bit(ac);
            }
            break;
        default:
            LOG_W("%s: unknown channel %lu", __func__, channel_idx);
            break;
    }

    if (event) {
        ke_evt_set(event);
    }

    return push_len;
}
