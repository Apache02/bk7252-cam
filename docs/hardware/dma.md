# BK7252 General DMA

The general DMA controller at `0x00809000` (`GDMA_BASE_ADDR`) provides six
independent channels. Each channel performs a sequence of word-sized read and
write transactions between two endpoints, with a small internal accumulator
adapting between the source and destination peripheral-side widths. End-of-
transfer events are signalled through `IRQ_SOURCE_GDMA`.

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
| `[31:16]` | `transfer_length`     | hw performs `transfer_length + 1` dst writes |

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

A channel performs exactly **`config.transfer_length + 1`** dst writes. The
value `transfer_length = 0` therefore yields one write, not zero — there is no
way to express "zero writes" in hardware.

Each dst write commits `2^dst_data_width` bytes via a byte-enable mask into the
low lanes of a 32-bit word at the dst address, then **advances the dst address
by +4 regardless of width**:

| `dst_dw` | BE mask  | Useful bytes per write |
| -------- | -------- | ---------------------- |
| `0` (8b) | `0001`   | 1                      |
| `1` (16b)| `0011`   | 2                      |
| `2` (32b)| `1111`   | 4                      |

Each src read fetches a 32-bit word at the src address but only places the
**low `2^src_data_width` bytes** into the internal accumulator (upper bytes
discarded). The src address advances by +4 per read when `src_addr_inc = 1`,
and stays put otherwise.

The accumulator behaves as a shift FIFO: reads append bytes from the bottom,
writes drain `2^dst_data_width` bytes from the bottom. A new read is issued
when the next planned write would find fewer than `2^dst_data_width` bytes in
the accumulator.

**The SDK names `8BIT` / `16BIT` / `32BIT` describe peripheral-side lane width,
not memory throughput.** For memory the relationship is inverse: the wider the
`dw`, the more useful bytes per bus cycle.

### Throughput implication

For memory-to-memory copy, `src_dw = dst_dw = 32BIT` (BE = `1111` both sides,
+4 increment with all 4 lanes active) gives full bus utilisation:
`ceil(N / 4)` reads and `ceil(N / 4)` writes for N bytes. Using `8BIT/8BIT`
runs roughly 4× slower and leaves three-byte gaps in dst between meaningful
bytes.

### Tail artefact

For some sizes the last dst write contains a trailing zero byte — e.g.
`src_dw = dst_dw = 2` with `size = 2` produces `aa aa aa 00` rather than
`aa aa aa aa`. Hypothesis: the accumulator is under-filled on the final
transaction. Not investigated further; does not occur in the `dw = 8 / 8`
memcpy case.

### Address alignment

Behaviour with unaligned src / dst addresses at `dw = 32` is not characterised
— hw may mask the low address bits, write to unintended bytes, or behave
unpredictably. All verified tests used 4-byte-aligned buffers.

## Operation sequence

1. Reserve a channel index `n` in `0..5`.
2. Optionally clear `config = 0` to drop any residual enable.
3. Program endpoint addresses: `src_start_addr` and `dst_start_addr`.
   Loop addresses can be left at `0` for single-shot mode.
4. Program `mux_reqs` with `src_req` / `dst_req` (use `0` for DTCM on either
   side).
5. Program `config` with desired widths, increments, `fin_int_enable`,
   `transfer_length = writes_needed - 1`, and `enable = 1` (or write `enable`
   separately in a follow-up store to start the transfer).
6. Wait for completion. Two equivalent ways:
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

- The hardware compresses the "configure" and "start" actions into the
  `enable` bit. Writing `config` with `enable = 1` in one store starts the
  transfer immediately; writing it with `enable = 0` programs the channel
  without starting it, and a subsequent store that sets `enable = 1`
  triggers the transfer using the previously programmed values.
- `enable` is self-clearing on completion. No additional acknowledgement is
  required to ready the channel for reuse beyond optionally clearing
  `int_status.fin_status[n]`.
- `int_counts[n].fin_count` (bits 24..27) and `half_fin_count` (bits 28..31)
  are sticky 4-bit IRQ counters — incremented by hw on each finish/half-
  finish, never auto-cleared, wrap at 16. Useful for debug, not for driver
  logic.
- `int_counts[n].remain_len` (bits 0..16) reports the number of dst writes
  remaining, in units of `2^dst_data_width` bytes.
- `src_rd_addr[n]` and `dst_wr_addr[n]` are internal counters with a
  hardware-specific encoding; they do not map to the current source/dest
  address in a straightforward way. Useful for debug only.
- `src_pause_addr[n]` and `dst_pause_addr[n]` capture addresses at pause
  events; not exercised.
- `prio_mode.fixed_priority = 0` selects round-robin arbitration and is
  the most useful default; in that mode `channel_priority` in `config` is
  ignored. With `fixed_priority = 1`, channels with higher
  `channel_priority` (0..3) are serviced first.
- Loop mode (`repeat_mode = 1` with `*_addr_loop = 1` and the four
  `*_loop_*_addr` registers) was not exercised; it is intended for
  continuous peripheral streaming (e.g. DVP → JPEG → RAM).
- `mux_reqs.src_rd_intval` and `dst_wr_intval` (rate-limit dividers, per
  SDK) and `dtcm_wr_wait_word` have not been characterised.