#ifndef _HARDWARE_GDMA_H
#define _HARDWARE_GDMA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <errno.h> // used for error codes

// Total number of channels exposed by the General DMA controller.
#define GDMA_NUM_CHANNELS (6)

typedef enum {
    GDMA_MODE_DTCM    = 0x0, // memory
    GDMA_MODE_HSSPI   = 0x1,
    GDMA_MODE_AUDIO   = 0x2,
    GDMA_MODE_SDIO    = 0x3,
    GDMA_MODE_UART1   = 0x4,
    GDMA_MODE_UART2   = 0x5,
    GDMA_MODE_I2S     = 0x6,
    GDMA_MODE_GSPI    = 0x7,
    GDMA_MODE_JPEG    = 0x8,
    GDMA_MODE_PSRAM_V = 0x9,
    GDMA_MODE_PSRAM_A = 0xA,
    GDMA_MODE_RESERVE = 0xB,
} gdma_mode_t;

// Peripheral-lane width values for src_data_width / dst_data_width.
// Number is the count of useful bytes per src read / dst write.
// NOTE: this does NOT change the +4 address increment per transaction.
// For DTCM->DTCM memcpy use GDMA_DATA_WIDTH_32 on both sides to get full
// bus utilization (BE=1111, +4 per transaction, 4 useful bytes per write).
// Narrower widths are SLOWER for memcpy because dst-address still advances
// +4 per write but fewer bytes are committed per word slot, leaving gaps.
#define GDMA_DATA_WIDTH_8  (0)
#define GDMA_DATA_WIDTH_16 (1)
#define GDMA_DATA_WIDTH_32 (2)

typedef void (gdma_int_handler_fn)(int);

// One side of a transfer: where data comes from or goes to.
typedef struct {
    gdma_mode_t mode; // GDMA_MODE_DTCM for plain memory; peripheral lines untested.
    uint32_t    addr; // 32-bit start address (or peripheral data register).
    uint32_t    loop_addr;
    uint32_t    loop_end_addr;
    bool        incr; // true = address advances by +4 per transaction; false = stays.
    uint8_t     dw;   // GDMA_DATA_WIDTH_8 / _16 / _32. Useful bytes per transaction.
} gdma_endpoint_t;

// Full transfer description for gdma_configure(). Transfer size is supplied
// separately, to gdma_start()/gdma_run() (see comment there).
typedef struct {
    gdma_endpoint_t src;
    gdma_endpoint_t dst;
    gdma_int_handler_fn * finish;
    gdma_int_handler_fn * h_finish;
} gdma_config_t;

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Channel reservation
// ============================================================================
// Reserved channels are owned by the caller until released. Currently there is
// no distinction between exclusive subsystem use and short-lived borrowing -
// callers cooperate. A future task manager (when added) will sit on top of
// reserved channels; subsystems that want a private channel will reserve it
// once at init and never release.

// Reserve any free channel. Returns channel index 0..GDMA_NUM_CHANNELS-1 or a
// negative errno. Not ISR-safe.
int gdma_reserve_channel(void);

int gdma_reserve_specific_channel(int ch);

// Release a previously reserved channel. The channel must not be busy
// (caller's responsibility to wait via gdma_wait/gdma_busy first).
// Not ISR-safe.
void gdma_release_channel(int ch);

// ============================================================================
// Low-level per-channel control
// ============================================================================
// All functions below operate on an already-reserved channel. Behavior on an
// unreserved channel is undefined (or, in debug builds, returns an error).

// Program the channel registers from cfg. Does NOT enable the channel.
// Use gdma_start() to begin the transfer. Safe to call again to reconfigure
// an idle (non-busy) channel without re-reserving.
// Returns 0 on success or negative errno.
int gdma_configure(int ch, const gdma_config_t *cfg);

// Set the transfer size and enable the channel. Transfer begins immediately.
// Channel must have been configured via gdma_configure() first. Calling on a
// busy channel is undefined. Returns 0 on success or negative errno if size
// is invalid, in which case the channel is left disabled.
//
// size is the total byte count to transfer, regardless of src.dw / dst.dw
// (empirically derived hardware model, not documented by the SDK — see
// docs/hardware/dma.md). The number of dst writes performed is
// ceil(size / 2^dst.dw), NOT size itself: with GDMA_DATA_WIDTH_32 a request
// for `size` bytes only issues size/4 writes. dst_addr_inc always advances
// the destination address by +4 per write regardless of dst.dw, so narrower
// widths leave gaps between the bytes actually written (see dw comment on
// gdma_endpoint_t above) — unsuitable for a contiguous memcpy. Use
// GDMA_DATA_WIDTH_32 on both sides for DTCM-to-DTCM memcpy.
// Must be >= 1 (hw cannot express zero bytes; value 0 rejected).
// Max 65536 (transfer_length is 16-bit, hw stores size-1).
int gdma_start(int ch, size_t size);

// Disable the channel mid-transfer. The internal accumulator and address
// counters are NOT reset; the next gdma_configure() will reprogram them.
// Use only for cancellation - normal completion does not require this.
void gdma_stop(int ch);

// True while the channel is actively transferring.
bool gdma_busy(int ch);

// Block until the channel finishes or timeout_ms elapses. Returns 0 on finish
// (immediately if idle), -ETIMEDOUT if the channel was still busy at the deadline
// (it is then stopped), -ENODEV on an invalid channel, -ENOMEM if no timeout could
// be created. Sleeps via sched_yield() while IRQ_SOURCE_GDMA is enabled at the ICU
// level and the channel's finish interrupt is enabled; otherwise busy-polls. The
// timeout expires only while CPU interrupts are enabled; INFINITE_TIMEOUT waits
// without a limit and does not depend on them.
int gdma_wait(int ch, uint32_t timeout_ms);

// ============================================================================
// Convenience: configure + start in one call. Equivalent to:
//   gdma_configure(channel, cfg); gdma_start(channel, size);
// Returns 0 on success or negative errno (in which case channel is left
// configured but not started, or not configured at all on early errors).
// ============================================================================
int gdma_run(int ch, const gdma_config_t *cfg, size_t size);

#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_GDMA_H
