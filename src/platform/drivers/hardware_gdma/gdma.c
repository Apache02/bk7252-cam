#include "hardware/gdma.h"
#include "hardware/intc.h"
#include "platform/init.h"
#include "gdma_regs.h"

#include <stdio.h>


// Bitmask of currently reserved channels. Bit N set => channel N is owned by
// some caller. Modified by gdma_reserve_channel / gdma_release_channel.
static uint32_t g_reserved_channels = 0;

// TODO(spinlock): protect concurrent reserve/release once a project-wide
// spinlock_t exists. For now: single-threaded use only (bare-metal app, or
// reservation only from one FreeRTOS task at init time).
// static spinlock_t g_reserve_lock = {0};



static void gdma_isr(void) {
    typeof(hw_gdma->int_status) status;
    status.v = hw_gdma->int_status.v;

    // ack everything that fired (write-1-to-clear)
    hw_gdma->int_status.v = status.v;
}

static void gdma_reset() {
    // disable every channel
    for (int ch = 0; ch < GDMA_NUM_CHANNELS; ch++) {
        hw_gdma->channels[ch].config.v = 0;
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

static void gdma_fini(void) {
    gdma_reset();
}

INIT_AT(gdma_init, 03);
FINI_AT(gdma_fini, 03);


// ============================================================================
// Channel reservation
// ============================================================================

int gdma_reserve_channel(void) {
    // spinlock_acquire(&g_reserve_lock);
    for (int ch = 0; ch < GDMA_NUM_CHANNELS; ch++) {
        if (!(g_reserved_channels & (1u << ch))) {
            g_reserved_channels |= (1u << ch);
            // spinlock_release(&g_reserve_lock);
            return ch;
        }
    }
    // spinlock_release(&g_reserve_lock);
    return GDMA_ERROR_FREE_CHANNEL_NOT_FOUND;
}

void gdma_release_channel(int channel) {
    if (channel < 0 || channel >= GDMA_NUM_CHANNELS) {
        return;
    }

    // If channel is still running, stop it before releasing. Caller should
    // have waited for completion, but better safe than to leave hw enabled
    // pointing at memory the next owner will reuse.
    if (gdma_busy(channel)) {
        gdma_stop(channel);
    }

    // spinlock_acquire(&g_reserve_lock);
    g_reserved_channels &= ~(1u << channel);
    // spinlock_release(&g_reserve_lock);
}


// ============================================================================
// Low-level per-channel control
// ============================================================================

int gdma_configure(int channel, const gdma_config_t *cfg) {
    if (channel < 0 || channel >= GDMA_NUM_CHANNELS) {
        return GDMA_ERROR_INVALID_CHANNEL;
    }
    if (!(g_reserved_channels & (1u << channel))) {
        return GDMA_ERROR_CHANNEL_NOT_RESERVED;
    }
    if (cfg == NULL || cfg->size == 0 || cfg->size > 0x10000) {
        // size must fit in 16-bit transfer_length after subtracting 1.
        // size=0 is rejected because hw cannot express "zero writes".
        return GDMA_ERROR_INVALID_CONFIG;
    }

    // Reset channel control register before reprogramming. Clears any
    // residual enable bit so we don't accidentally start mid-write.
    hw_gdma->channels[channel].config.v = 0;

    hw_gdma->channels[channel].src_start_addr = cfg->src.addr;
    hw_gdma->channels[channel].src_loop_start_addr = 0;
    hw_gdma->channels[channel].src_loop_end_addr = 0;

    hw_gdma->channels[channel].dst_start_addr = cfg->dst.addr;
    hw_gdma->channels[channel].dst_loop_start_addr = 0;
    hw_gdma->channels[channel].dst_loop_end_addr = 0;

    typeof(hw_gdma->channels[channel].mux_reqs) request = {
        .src_req = cfg->src.mode,
        .dst_req = cfg->dst.mode,
        .dtcm_wr_wait_word = 0,
        .src_rd_intval = 0,
        .dst_wr_intval = 0,
    };
    hw_gdma->channels[channel].mux_reqs = request;

    typeof(hw_gdma->channels[channel].config) config = {
        .enable = 0,                              // started separately via gdma_start
        .fin_int_enable = 1,
        .half_fin_int_enable = 0,
        .repeat_mode = 0,
        .src_data_width = cfg->src.dw,
        .dst_data_width = cfg->dst.dw,
        .src_addr_inc = cfg->src.incr,
        .dst_addr_inc = cfg->dst.incr,
        .src_addr_loop = 0,
        .dst_addr_loop = 0,
        .channel_priority = 0,
        .transfer_length = cfg->size - 1,         // hw stores (size - 1); see model in gdma_regs.h
    };
    hw_gdma->channels[channel].config = config;

    return 0;
}

void gdma_start(int channel) {
    if (channel < 0 || channel >= GDMA_NUM_CHANNELS) {
        return;
    }
    hw_gdma->channels[channel].config.enable = 1;
}

void gdma_stop(int channel) {
    if (channel < 0 || channel >= GDMA_NUM_CHANNELS) {
        return;
    }
    hw_gdma->channels[channel].config.enable = 0;
}

bool gdma_busy(int channel) {
    if (channel < 0 || channel >= GDMA_NUM_CHANNELS) {
        return false;
    }
    return hw_gdma->channels[channel].config.enable != 0;
}

void gdma_wait(int channel) {
    if (channel < 0 || channel >= GDMA_NUM_CHANNELS) {
        return;
    }
    while (hw_gdma->channels[channel].config.enable) {
        // busy-poll; hw clears enable on completion
    }
}


// ============================================================================
// Convenience
// ============================================================================

int gdma_run(int channel, const gdma_config_t *cfg) {
    int rc = gdma_configure(channel, cfg);
    if (rc != 0) {
        return rc;
    }
    gdma_start(channel);
    return 0;
}