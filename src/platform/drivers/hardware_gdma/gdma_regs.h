#pragma once

#include <stdint.h>
#include "register_defs.h"


#define GDMA_BASE_ADDR              (0x00809000)
#define GDMA_CHANNELS               (6)


typedef volatile struct {
    union {
        struct {
            uint32_t enable: 1;                     // [0]
            uint32_t fin_int_enable: 1;             // [1]
            uint32_t half_fin_int_enable: 1;        // [2]
            uint32_t repeat_mode: 1;                // [3] 0: single-shot, 1: loop using *_loop_*_addr (NOT TESTED; semantics from SDK)
            uint32_t src_data_width: 2;             // [5:4] 0=8b, 1=16b, 2=32b useful bytes per src read (see notes below)
            uint32_t dst_data_width: 2;             // [7:6] 0=8b, 1=16b, 2=32b useful bytes per dst write (see notes below)
            uint32_t src_addr_inc: 1;               // [8] when 1, src address advances by +4 bytes per read (always +4, independent of src_data_width)
            uint32_t dst_addr_inc: 1;               // [9] when 1, dst address advances by +4 bytes per write (always +4, independent of dst_data_width)
            uint32_t src_addr_loop: 1;              // [10] (NOT TESTED)
            uint32_t dst_addr_loop: 1;              // [11] (NOT TESTED)
            uint32_t channel_priority: 2;           // [13:12] 0..3, used only when prio_mode.fixed_priority = 1
            uint32_t reserved_14_15: 2;             // [14:15]
            uint32_t transfer_length: 16;           // [31:16] number of dst writes minus 1. HW performs (transfer_length + 1) dst writes.
                                                    //         Cannot express zero writes; value 0 yields exactly one write.
                                                    //         Each write commits 2^dst_data_width bytes to dst.
        };

        uint32_t v;
    } config;

    uint32_t dst_start_addr;
    uint32_t src_start_addr;
    uint32_t dst_loop_end_addr;                     // (NOT TESTED)
    uint32_t dst_loop_start_addr;                   // (NOT TESTED)
    uint32_t src_loop_end_addr;                     // (NOT TESTED)
    uint32_t src_loop_start_addr;                   // (NOT TESTED)

    union {
        struct {
            uint32_t src_req: 4;                    // [3:0] peripheral source request line, 0 for M2M (DTCM). Only DTCM tested.
            uint32_t dst_req: 4;                    // [7:4] peripheral destination request line, 0 for M2M (DTCM). Only DTCM tested.
            uint32_t dtcm_wr_wait_word: 1;          // [8] (NOT INVESTIGATED)
            uint32_t reserved_9_11: 3;              // [11:9]
            uint32_t src_rd_intval: 4;              // [15:12] cycles between src reads (from SDK; NOT TESTED)
            uint32_t dst_wr_intval: 4;              // [19:16] cycles between dst writes (from SDK; NOT TESTED)
            uint32_t reserved_20_31: 12;            // [31:20]
        };

        uint32_t v;
    } mux_reqs;
} hw_gdma_channel_t;


// ============================================================================
// Transfer model (verified empirically; not documented in datasheet)
// ============================================================================
//
// The controller performs exactly (config.transfer_length + 1) dst writes.
// Each dst write commits 2^dst_data_width bytes; the dst address advances by
// +4 bytes per write regardless of width (byte-enable mask selects which lanes
// of the 32-bit word are written: 0001, 0011, 1111 for widths 0, 1, 2).
//
// Each src read fetches a 32-bit word from src_addr; only the low
// 2^src_data_width bytes are pushed into an internal accumulator (upper bytes
// discarded). src address advances by +4 per read when src_addr_inc=1.
//
// The accumulator behaves as a shift FIFO: reads append low bytes from
// the bottom; writes drain 2^dst_data_width bytes from the bottom. A new read
// is triggered when accumulator depth < 2^dst_data_width at the moment the
// next planned write needs data.
//
// Consequence: data_width names (8/16/32) describe peripheral-side lane width,
// NOT memory throughput. For DTCM->DTCM memcpy, prefer 8BIT/8BIT
// (BE=1111 both sides, +4 per transaction, full bus utilization).
// Using 32BIT for memcpy gives 1 useful byte per 4-byte word slot in dst
// and runs ~4x slower.
// ============================================================================


typedef volatile struct {
    hw_gdma_channel_t channels[GDMA_CHANNELS];      // word 0x00..0x2F: 8 words per channel x 6 channels

    union {
        struct {
            uint32_t remain_len: 17;                // [16:0] dst writes remaining (one unit = 2^dst_data_width bytes)
            uint32_t reserved_17_23: 7;             // [23:17]
            uint32_t fin_count: 4;                  // [27:24] sticky finish-IRQ counter, never auto-clears.
                                                    //         Increments by 1 per fin interrupt; wraps at 16.
            uint32_t half_fin_count: 4;             // [31:28] same as fin_count but for half-finish IRQ.
        };

        uint32_t v;
    } int_counts[GDMA_CHANNELS];                    // word 0x30..0x35

    uint32_t reserved_word_0x36;                    // word 0x36

    union {
        struct  {
            uint32_t fixed_priority: 1;             // [0] 0: round-robin arbitration across channels,
                                                    //     1: fixed priority taken from channel_priority of each channel
            uint32_t reserved_1_31: 31;             // [31:1]
        };

        uint32_t v;
    } prio_mode;                                    // word 0x37

    union {
        struct {
            uint32_t fin_status: 6;                 // [5:0] per-channel finish flag, write 1 to clear
            uint32_t reserved_6_7: 2;               // [7:6]
            uint32_t half_fin_status: 6;            // [13:8] per-channel half-finish flag, write 1 to clear
            uint32_t reserved_14_31: 18;            // [31:14]
        };

        uint32_t v;
    } int_status;                                   // word 0x38

    uint32_t reserved_word_0x39_0x3F[7];            // word 0x39..0x3F

    uint32_t src_pause_addr[GDMA_CHANNELS];         // word 0x40..0x45
    uint32_t reserved_word_0x46_0x47[2];            // word 0x46..0x47
    uint32_t dst_pause_addr[GDMA_CHANNELS];         // word 0x48..0x4D
    uint32_t reserved_word_0x4E_0x4F[2];            // word 0x4E..0x4F
    uint32_t src_rd_addr[GDMA_CHANNELS];            // word 0x50..0x55. Internal src read counter; does NOT
                                                    // map to the current source address in a straightforward way.
                                                    // Debug only.
    uint32_t reserved_word_0x56_0x57[2];            // word 0x56..0x57
    uint32_t dst_wr_addr[GDMA_CHANNELS];            // word 0x58..0x5D. Internal dst write counter; same caveat
                                                    // as src_rd_addr. Debug only.
} hw_gdma_t;


#define hw_gdma                     ((volatile hw_gdma_t *) GDMA_BASE_ADDR)
