# BK7252 General DMA

The general DMA controller at `0x00809000` (`GDMA_BASE_ADDR`) provides six
independent channels. Each channel transfers a specified number of bytes between
two endpoints, reading source words and writing destination words through an
internal byte accumulator that adapts between the source and destination widths.
End-of-transfer events are signalled through `IRQ_SOURCE_GDMA`.

Endpoints can be plain DTCM memory (`mode = DTCM`) or a peripheral request line
(HSSPI, AUDIO, SDIO, UART1/2, I2S, GSPI, JPEG, PSRAM); only `DTCM` has been
characterised in detail.

## Register map

### Per-channel block (8 words; channel `n` at base + `0x20 * n`)

| Offset (words) | Name                   | Purpose                                |
| -------------- | ---------------------- | -------------------------------------- |
| `0x00`         | `config`               | enable / int-enable / widths / increments / loop / priority / `transfer_length` |
| `0x01`         | `dst_start_addr`       | Destination start address              |
| `0x02`         | `src_start_addr`       | Source start address                   |
| `0x03`         | `dst_loop_end_addr`    | Destination loop end (loop mode only)  |
| `0x04`         | `dst_loop_start_addr`  | Destination loop start                 |
| `0x05`         | `src_loop_end_addr`    | Source loop end                        |
| `0x06`         | `src_loop_start_addr`  | Source loop start                      |
| `0x07`         | `mux_reqs`             | `src_req` / `dst_req` peripheral lines / read-write intervals |

### Global block (starts at base + `0xC0`, i.e. word `0x30`)

| Offset (words) | Name                | Purpose                                  |
| -------------- | ------------------- | ---------------------------------------- |
| `0x30..0x35`   | `int_counts[6]`     | Per-channel `remain_len`, sticky `fin_count` / `half_fin_count` |
| `0x37`         | `prio_mode`         | `bit0`: 0 = round-robin, 1 = fixed priority |
| `0x38`         | `int_status`        | `bits0..5`: per-channel fin flags, `bits8..13`: half-fin (write 1 to clear) |
| `0x40..0x45`   | `src_pause_addr[6]` | Pause-point capture (telemetry)          |
| `0x48..0x4D`   | `dst_pause_addr[6]` | Pause-point capture (telemetry)          |
| `0x50..0x55`   | `src_rd_addr[6]`    | Internal source read counter (telemetry) |
| `0x58..0x5D`   | `dst_wr_addr[6]`    | Internal destination write counter (telemetry) |

### `config` bits

| Bits      | Name                  | Purpose                                 |
| --------- | --------------------- | --------------------------------------- |
| `[0]`     | `enable`              | 1 = channel runs; hw clears at completion |
| `[1]`     | `fin_int_enable`      | Generate IRQ on finish                  |
| `[2]`     | `half_fin_int_enable` | Generate IRQ at half-finish             |
| `[3]`     | `repeat_mode`         | 0 = single-shot, 1 = loop (loop addresses) |
| `[5:4]`   | `src_data_width`      | `0=8b`, `1=16b`, `2=32b` (useful bytes per src read) |
| `[7:6]`   | `dst_data_width`      | `0=8b`, `1=16b`, `2=32b` (useful bytes per dst write) |
| `[8]`     | `src_addr_inc`        | Advance source address by +4 per read   |
| `[9]`     | `dst_addr_inc`        | Advance destination address by +4 per write |
| `[10]`    | `src_addr_loop`       | Enable source loop wrap                 |
| `[11]`    | `dst_addr_loop`       | Enable destination loop wrap            |
| `[13:12]` | `channel_priority`    | 0..3, consulted only when `prio_mode.fixed_priority = 1` |
| `[31:16]` | `transfer_length`     | Total bytes to transfer minus 1; hw transfers `transfer_length + 1` bytes |

### `mux_reqs` bits

| Bits      | Name                | Purpose                                |
| --------- | ------------------- | -------------------------------------- |
| `[3:0]`   | `src_req`           | Source peripheral request line (`0` = DTCM/memory) |
| `[7:4]`   | `dst_req`           | Destination peripheral request line     |
| `[8]`     | `dtcm_wr_wait_word` | Function not characterised             |
| `[15:12]` | `src_rd_intval`     | Cycles between source reads (rate-limit, not characterised) |
| `[19:16]` | `dst_wr_intval`     | Cycles between destination writes (rate-limit, not characterised) |

### Peripheral request line values (`src_req` / `dst_req`)

| Value | Endpoint   | Value | Endpoint   |
| ----- | ---------- | ----- | ---------- |
| `0x0` | DTCM       | `0x6` | I2S        |
| `0x1` | HSSPI      | `0x7` | GSPI       |
| `0x2` | AUDIO      | `0x8` | JPEG       |
| `0x3` | SDIO       | `0x9` | PSRAM_V    |
| `0x4` | UART1      | `0xA` | PSRAM_A    |
| `0x5` | UART2      | `0xB` | (reserved) |

## Transfer model

**This is not described in the SDK — without it, hardware behaviour appears
random. The model below was derived empirically and matches every test case.**

A channel transfers exactly **`config.transfer_length + 1` bytes**. The value
`transfer_length = 0` therefore transfers one byte, not zero — there is no way
to express "zero bytes" in hardware.

**`transfer_length` counts bytes, not reads or writes.** The number of dst
writes is `ceil((transfer_length + 1) / 2^dst_data_width)`; the number of src
reads is `ceil((transfer_length + 1) / 2^src_data_width)`.

### Source reads

Each src read fetches a full 32-bit word from the src address, but only the
**low `2^src_data_width` bytes** are placed into the internal accumulator;
upper bytes are discarded. The src address always advances by +4 per read when
`src_addr_inc = 1`.

| `src_dw` | Useful bytes per read |
| -------- | --------------------- |
| `0` (8b) | 1 (byte 0 only)       |
| `1` (16b)| 2 (bytes 1:0)         |
| `2` (32b)| 4 (bytes 3:0)         |

### Destination writes

Each dst write drains `2^dst_data_width` bytes from the accumulator and commits
them via byte-enable mask into the low lanes of a 32-bit word at the dst
address. The dst address always advances by +4 per write when `dst_addr_inc = 1`.

| `dst_dw` | BE mask  | Useful bytes per write |
| -------- | -------- | ---------------------- |
| `0` (8b) | `0001`   | 1                      |
| `1` (16b)| `0011`   | 2                      |
| `2` (32b)| `1111`   | 4                      |

### Internal Accumulator

The accumulator is a 32-bit shift FIFO. Src reads append bytes at the top; dst
writes drain bytes from the bottom. A new src read is issued whenever the
accumulator holds fewer bytes than the next dst write requires.

**`src_dw` and `dst_dw` are independent.** Any combination is valid:

- `src_dw=2, dst_dw=0`: one read fills the accumulator with 4 bytes; four writes
  drain them one byte at a time.
- `src_dw=0, dst_dw=2`: four reads each contribute 1 byte to the accumulator;
  one write drains all four bytes in a single 32-bit store.
- `src_dw=1, dst_dw=1`: one read, one write; 2 bytes transferred per cycle.

### Throughput

For memory-to-memory copy, `src_dw = dst_dw = 2` (32-bit both sides) gives
full bus utilisation: `ceil(N / 4)` reads and `ceil(N / 4)` writes for N
bytes. Using `src_dw = dst_dw = 0` runs roughly 4× slower — each bus cycle
moves one useful byte, and dst has three-byte gaps between meaningful bytes.

### Tail artefact

For some sizes the last dst write contains a trailing zero byte — e.g.
`src_dw = dst_dw = 2` with `size = 2` produces `aa aa aa 00` rather than
`aa aa aa aa`. The accumulator is under-filled on the final transaction;
the DMA always issues a full-width write with the available BE mask
(0001/0011/1111), so unfilled accumulator bytes are committed as 0x00.



### Address alignment

Behaviour with unaligned src / dst addresses at `dw = 2` (32-bit) is not
characterised — hardware may mask the low address bits, write to unintended
bytes, or behave unpredictably. Use 4-byte-aligned buffers.

## Operation sequence

1. Reserve a channel index `n` in `0..5`.
2. Optionally write `config = 0` to drop any residual enable.
3. Write `src_start_addr` and `dst_start_addr`.
   Loop addresses can be left at `0` for single-shot mode.
4. Write `mux_reqs` with `src_req` / `dst_req` (use `0` for DTCM on either
   side).
5. Write `config` with desired widths, increments, `fin_int_enable`,
   `transfer_length = byte_count - 1`, and `enable = 1` to start the transfer.
   Alternatively write `config` with `enable = 0` to program without starting,
   then write `enable = 1` separately to trigger.
6. Wait for completion — two equivalent ways:
    - Poll `channels[n].config.enable` — hw clears it to `0` on the final write.
    - Wait for the channel's bit in `int_status.fin_status` and write 1 to clear.

## Interrupts

`IRQ_SOURCE_GDMA` is a single shared line for all six channels. The ISR must
read `int_status` to discover which channels finished:

- `int_status.fin_status[n]` — channel `n` completed its transfer. Write 1
  to clear.
- `int_status.half_fin_status[n]` — channel `n` reached the half-point of
  its transfer (chiefly meaningful in loop mode). Write 1 to clear.

A channel's flag stays asserted until acknowledged, so failing to clear it
keeps `IRQ_SOURCE_GDMA` high.

## Notes

- `enable` is self-clearing on completion. No additional acknowledgement is
  required to ready the channel for reuse beyond optionally clearing
  `int_status.fin_status[n]`.
- `int_counts[n].fin_count` (bits 24..27) and `half_fin_count` (bits 28..31)
  are sticky 4-bit IRQ counters — incremented by hw on each finish/half-finish,
  never auto-cleared, wrap at 16. Useful for debug, not for driver logic.
- `int_counts[n].remain_len` (bits 0..16) reports the number of dst writes
  remaining.
- `src_rd_addr[n]` and `dst_wr_addr[n]` are internal counters with a
  hardware-specific encoding; they do not map to the current source/dest
  address in a straightforward way. Useful for debug only.
- `src_pause_addr[n]` and `dst_pause_addr[n]` capture addresses at pause
  events; not exercised.
- `prio_mode.fixed_priority = 0` selects round-robin arbitration and is the
  most useful default; in that mode `channel_priority` in `config` is ignored.
  With `fixed_priority = 1`, channels with higher `channel_priority` (0..3)
  are serviced first.
- Loop mode (`repeat_mode = 1` with `*_addr_loop = 1` and the four
  `*_loop_*_addr` registers) was not exercised; it is intended for continuous
  peripheral streaming (e.g. DVP → JPEG → RAM).
- `mux_reqs.src_rd_intval`, `dst_wr_intval`, and `dtcm_wr_wait_word` have not
  been characterised. Leave at `0`.