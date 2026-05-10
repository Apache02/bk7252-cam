#ifndef _HARDWARE_GDMA_H
#define _HARDWARE_GDMA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


// Total number of channels exposed by the General DMA controller.
#define GDMA_NUM_CHANNELS               (6)

typedef enum {
    GDMA_MODE_DTCM = 0x0, // memory
    GDMA_MODE_HSSPI = 0x1,
    GDMA_MODE_AUDIO = 0x2,
    GDMA_MODE_SDIO = 0x3,
    GDMA_MODE_UART1 = 0x4,
    GDMA_MODE_UART2 = 0x5,
    GDMA_MODE_I2S = 0x6,
    GDMA_MODE_GSPI = 0x7,
    GDMA_MODE_JPEG = 0x8,
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
#define GDMA_DATA_WIDTH_8           (0)
#define GDMA_DATA_WIDTH_16          (1)
#define GDMA_DATA_WIDTH_32          (2)


enum {
    GDMA_ERROR_UNSPECIFIED = -1,
    GDMA_ERROR_FREE_CHANNEL_NOT_FOUND = -2,
    GDMA_ERROR_INVALID_CHANNEL = -3,
    GDMA_ERROR_CHANNEL_NOT_RESERVED = -4,
    GDMA_ERROR_INVALID_CONFIG = -5,
};


// One side of a transfer: where data comes from or goes to.
typedef struct {
    gdma_mode_t mode;       // GDMA_MODE_DTCM for plain memory; peripheral lines untested.
    uint32_t addr;          // 32-bit start address (or peripheral data register).
    bool incr;              // true = address advances by +4 per transaction; false = stays.
    uint8_t dw;             // GDMA_DATA_WIDTH_8 / _16 / _32. Useful bytes per transaction.
} gdma_endpoint_t;


// Full transfer description for gdma_configure().
typedef struct {
    gdma_endpoint_t src;
    gdma_endpoint_t dst;
    size_t size;            // Number of dst writes to perform. Each commits 2^dst.dw bytes.
                            // Must be >= 1 (hw cannot express zero writes; value 0 rejected).
                            // Max 65536 (transfer_length is 16-bit, hw stores size-1).
                            // For byte-count memcpy with dw=8/8 use size = byte_count.
                            // For dw=8/16 use size = ceil(byte_count / 2). Etc.
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
// negative GDMA_ERROR_* code. Not ISR-safe.
int gdma_reserve_channel(void);

// Release a previously reserved channel. The channel must not be busy
// (caller's responsibility to wait via gdma_wait/gdma_busy first).
// Not ISR-safe.
void gdma_release_channel(int channel);


// ============================================================================
// Low-level per-channel control
// ============================================================================
// All functions below operate on an already-reserved channel. Behavior on an
// unreserved channel is undefined (or, in debug builds, returns an error).

// Program the channel registers from cfg. Does NOT enable the channel.
// Use gdma_start() to begin the transfer. Safe to call again to reconfigure
// an idle (non-busy) channel without re-reserving.
// Returns 0 on success or negative GDMA_ERROR_*.
int gdma_configure(int channel, const gdma_config_t *cfg);

// Enable the channel. Transfer begins immediately. Channel must have been
// configured via gdma_configure() first. Calling on a busy channel is undefined.
void gdma_start(int channel);

// Disable the channel mid-transfer. The internal accumulator and address
// counters are NOT reset; the next gdma_configure() will reprogram them.
// Use only for cancellation - normal completion does not require this.
void gdma_stop(int channel);

// True while the channel is actively transferring. Becomes false on the
// finish event. Polled by gdma_wait().
bool gdma_busy(int channel);

// Block (busy-poll) until the channel finishes. Returns immediately if idle.
// No timeout; the caller is responsible for not waiting on a stuck channel.
void gdma_wait(int channel);


// ============================================================================
// Convenience: configure + start in one call. Equivalent to:
//   gdma_configure(channel, cfg); gdma_start(channel);
// Returns 0 on success or negative GDMA_ERROR_* (in which case channel is
// left configured but not started, or not configured at all on early errors).
// ============================================================================
int gdma_run(int channel, const gdma_config_t *cfg);


#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_GDMA_H