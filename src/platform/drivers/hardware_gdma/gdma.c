#include "hardware/gdma.h"
#include "soc/gdma.h"
#include "soc/icu.h"
#include "hardware/intc.h"
#include "platform/init.h"
#include "platform/sched.h"
#include "platform/cpu.h"
#include "platform/timeout.h"

#include <stdio.h>


// Bitmask of currently reserved channels. Bit N set => channel N is owned by
// some caller. Modified by gdma_reserve_channel / gdma_release_channel.
static uint32_t             g_reserved_channels                  = 0;
static gdma_int_handler_fn *finish_handlers[GDMA_NUM_CHANNELS]   = {NULL};
static gdma_int_handler_fn *h_finish_handlers[GDMA_NUM_CHANNELS] = {NULL};

#define IS_CHANNEL_VALID(ch) (ch >= 0 && ch < GDMA_NUM_CHANNELS)


static void gdma_isr(void) {
    typeof(hw_gdma->int_status) status = {.v = hw_gdma->int_status.v};
    // ack everything that fired (write-1-to-clear)
    hw_gdma->int_status.v = status.v;

    uint32_t bits = status.fin_status, h_bits = status.half_fin_status;
    // bits = status.fin_status;
    for (int ch = 0; ch < GDMA_NUM_CHANNELS; ch++) {
        if ((1 << ch) & bits) {
            if (finish_handlers[ch]) finish_handlers[ch](ch);
        }
        if ((1 << ch) & h_bits) {
            if (h_finish_handlers[ch]) h_finish_handlers[ch](ch);
        }
    }
}

static void gdma_reset() {
    // disable every channel
    for (int ch = 0; ch < GDMA_NUM_CHANNELS; ch++) {
        hw_gdma->int_counts[ch].v      = 0;
        hw_gdma->channels[ch].config.v = 0;
        finish_handlers[ch]            = NULL;
        h_finish_handlers[ch]          = NULL;
    }
}

static void gdma_init(void) {
    gdma_reset();

    // ack any leftover interrupt flags (write-1-to-clear)
    hw_gdma->int_status.v = hw_gdma->int_status.v;

    // round-robin arbitration by default; channel_priority field in CONF is
    // ignored in this mode, all channels get equal share.
    hw_gdma->prio_mode.v = 0;

    intc_register_irq_handler(IRQ_SOURCE_GDMA, gdma_isr);
    intc_enable_irq_source(IRQ_SOURCE_GDMA);
}

static void gdma_fini(void) { gdma_reset(); }

INIT_AT(gdma_init, 03);
FINI_AT(gdma_fini, 03);

// ============================================================================
// Channel reservation
// ============================================================================

int gdma_reserve_channel(void) {
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    for (int ch = 0; ch < GDMA_NUM_CHANNELS; ch++) {
        if (!(g_reserved_channels & (1u << ch))) {
            g_reserved_channels |= (1u << ch);
            GLOBAL_INT_RESTORE();
            return ch;
        }
    }
    GLOBAL_INT_RESTORE();
    return -EBUSY;
}

int gdma_reserve_specific_channel(const int ch) {
    if (!IS_CHANNEL_VALID(ch)) {
        return -ENODEV;
    }
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    if (!(g_reserved_channels & (1u << ch))) {
        g_reserved_channels |= (1u << ch);
        GLOBAL_INT_RESTORE();
        return ch;
    }
    GLOBAL_INT_RESTORE();
    return -EBUSY;
}

void gdma_release_channel(const int ch) {
    if (!IS_CHANNEL_VALID(ch)) {
        return;
    }

    // If channel is still running, stop it before releasing. Caller should
    // have waited for completion, but better safe than to leave hw enabled
    // pointing at memory the next owner will reuse.
    if (gdma_busy(ch)) {
        gdma_stop(ch);
    }

    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    g_reserved_channels &= ~(1u << ch);
    GLOBAL_INT_RESTORE();
}

// ============================================================================
// Low-level per-channel control
// ============================================================================

int gdma_configure(const int ch, const gdma_config_t *cfg) {
    if (!IS_CHANNEL_VALID(ch)) {
        return -ENODEV;
    }
    if (!(g_reserved_channels & (1u << ch))) {
        return -EPERM;
    }
    if (cfg == NULL || cfg->src.interval > 15 || cfg->dst.interval > 15) {
        return -EINVAL;
    }

    const bool src_loop = (cfg->src.loop_addr && cfg->src.loop_end_addr);
    const bool dst_loop = (cfg->dst.loop_addr && cfg->dst.loop_end_addr);

    // Reset channel control register before reprogramming. Clears any
    // residual enable bit so we don't accidentally start mid-write.
    hw_gdma->channels[ch].config.v = 0;

    hw_gdma->channels[ch].src_start_addr      = cfg->src.addr;
    hw_gdma->channels[ch].src_loop_start_addr = src_loop ? cfg->src.loop_addr : 0;
    hw_gdma->channels[ch].src_loop_end_addr   = src_loop ? cfg->src.loop_end_addr : 0;

    hw_gdma->channels[ch].dst_start_addr      = cfg->dst.addr;
    hw_gdma->channels[ch].dst_loop_start_addr = dst_loop ? cfg->dst.loop_addr : 0;
    hw_gdma->channels[ch].dst_loop_end_addr   = dst_loop ? cfg->dst.loop_end_addr : 0;

    hw_write_fields(hw_gdma->channels[ch].mux_reqs,
        .src_req = cfg->src.mode,
        .dst_req = cfg->dst.mode,
        .src_rd_interval = cfg->src.interval,
        .dst_wr_interval = cfg->dst.interval,
    );

    // enable bit and transfer_length are left zero here; gdma_start() sets both
    // together once the transfer size is known.
    hw_write_fields(hw_gdma->channels[ch].config,
        .enable = 0,
        .fin_int_enable = cfg->finish ? 1 : 0,
        .half_fin_int_enable = cfg->h_finish ? 1 : 0,
        .repeat_mode = cfg->repeat ? 1 : 0,
        .src_data_width = cfg->src.dw,
        .dst_data_width = cfg->dst.dw,
        .src_addr_inc = cfg->src.incr,
        .dst_addr_inc = cfg->dst.incr,
        .dst_addr_loop = dst_loop ? 1 : 0,
        .src_addr_loop = src_loop ? 1 : 0,
        .channel_priority = 0,
    );

    finish_handlers[ch]   = cfg->finish;
    h_finish_handlers[ch] = cfg->h_finish;

    return 0;
}

int gdma_start(const int ch, const size_t size) {
    if (!IS_CHANNEL_VALID(ch)) {
        return -ENODEV;
    }
    if (size == 0 || size > 0x10000) {
        // size must fit in 16-bit transfer_length after subtracting 1.
        // size=0 is rejected because hw cannot express "zero writes".
        return -EINVAL;
    }

    typeof(hw_gdma->channels[ch].config) config = {.v = hw_gdma->channels[ch].config.v};
    config.transfer_length                      = size - 1;
    config.enable                               = 1;
    hw_gdma->channels[ch].config.v              = config.v;

    return 0;
}

int gdma_restart(const int ch, const uint32_t dst_addr, const size_t size) {
    if (!IS_CHANNEL_VALID(ch)) {
        return -ENODEV;
    }
    hw_gdma->channels[ch].config.enable  = 0;
    hw_gdma->channels[ch].dst_start_addr = dst_addr;
    return gdma_start(ch, size);
}

size_t gdma_transferred(const int ch) {
    if (!IS_CHANNEL_VALID(ch)) {
        return 0;
    }
    return hw_gdma->dst_wr_addr[ch] - hw_gdma->channels[ch].dst_start_addr;
}

void gdma_stop(const int ch) {
    if (!IS_CHANNEL_VALID(ch)) {
        return;
    }
    hw_gdma->channels[ch].config.enable = 0;
}

bool gdma_busy(const int ch) {
    if (!IS_CHANNEL_VALID(ch)) {
        return false;
    }
    return hw_gdma->channels[ch].config.enable != 0;
}

int gdma_wait(const int ch, const uint32_t timeout_ms) {
    if (!IS_CHANNEL_VALID(ch)) return -ENODEV;

    if (hw_gdma->channels[ch].config.enable) {
        struct timeout_t t;
        if (!create_timeout(&t, timeout_ms)) {
            return -ENOMEM;
        }
        // Avoid sched_yield()/WFI() if nothing will ever wake it.
        const bool can_wake = intc_irq_source_enabled(IRQ_SOURCE_GDMA) && hw_gdma->channels[ch].config.fin_int_enable;
        while (hw_gdma->channels[ch].config.enable) {
            if (is_timeout_finished(&t)) {
                hw_gdma->channels[ch].config.enable = 0;
                return -ETIMEDOUT;
            }

            if (can_wake) sched_yield();
        }
        free_timeout(&t);
    }

    // Ack this channel's finish flag ourselves - gdma_isr() only runs if CPU IRQ
    // happens to be enabled. Without this, an unacked flag stays asserted forever,
    // silently turning every later WFI()-based wait (this channel's and anyone
    // else's) into an immediate no-op instead of a real sleep.
    hw_write_fields(hw_gdma->int_status,
        .fin_status = (1u << ch)
    );

    return 0;
}

// ============================================================================
// Convenience
// ============================================================================

int gdma_run(const int ch, const gdma_config_t *cfg, const size_t size) {
    int rc = gdma_configure(ch, cfg);
    if (rc != 0) {
        return rc;
    }
    return gdma_start(ch, size);
}
