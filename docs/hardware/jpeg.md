# BK7252 Hardware JPEG Encoder / DVP Interface

The JPEG encoder block sits at `0x0080A000`. It is both the 8-bit DVP (Digital
Video Port) camera interface and a hardware MJPEG encoder: it drives the
sensor's master clock, receives the sensor's pixel stream over HSYNC/VSYNC/PCLK
plus 8 data lines, and compresses that stream to JPEG using an on-chip
quantization table.

DVP pins: MCLK (GPIO27, output to the sensor), PCLK (GPIO29, input from the
sensor), HSYNC (GPIO30), VSYNC (GPIO31), PXD0–PXD7 (GPIO32–GPIO39). GPIO28 is
not part of this group. All of them are configured through the GPIO block's
second-function mux, not through this block. PCLK for the capture path is
clocked from DPLL at 96 MHz; sensor MCLK output is divided down from that.

## Register map

| Offset (bytes) | Name                  | Purpose                                    |
| --------------- | --------------------- | ------------------------------------------- |
| `0x00`           | `ctrl0`               | Frame interrupt enables, MCLK divider       |
| `0x04`           | `ctrl1`               | Encoder enable, format, frame size, bit-rate control |
| `0x08`           | `target_byte_h`       | Max bytes per frame for bit-rate control    |
| `0x0C`           | `target_byte_l`       | Min bytes per frame for bit-rate control    |
| `0x10`           | —                      | Reserved                                    |
| `0x14`           | `rx_fifo_data`        | Encoded JPEG output FIFO. Read-only, DMA source port |
| `0x18`           | `status`               | Frame interrupt status, write-1-to-clear    |
| `0x1C`           | `byte_cnt_pfrm`       | Byte count of the last encoded frame. Read-only |
| `0x20`           | `rx_state`            | Output FIFO state. Read-only                |
| `0x24`–`0x7C`     | —                      | Reserved                                    |
| `0x80`–`0xFC`     | `quantization_table`  | 32-word quantization table RAM              |

### `ctrl0`

| Bits    | Name            | Purpose                                              |
| ------- | --------------- | ----------------------------------------------------- |
| `[1:0]` | —               | Reserved                                              |
| `[2]`   | `start_frm_int` | Start-of-frame interrupt enable                       |
| `[3]`   | `end_frm_int`   | End-of-frame interrupt enable                         |
| `[5:4]` | `div`           | MCLK divider: `0`=24 MHz, `1`=16 MHz, `2`=12 MHz, `3`=24 MHz |

### `ctrl1`

| Bits      | Name            | Purpose                                                       |
| --------- | --------------- | --------------------------------------------------------------|
| `[0]`     | —               | Reserved                                                       |
| `[1]`     | `video_byte_rev`| Byte order inside each word read from `rx_fifo_data`: `1` gives the stream in natural order (a JPEG starts with the word `0xE0FFD8FF`, bytes `FF D8 FF E0`); `0` reverses the bytes of every word |
| `[3:2]`   | `yuv_fmt_sel`   | Input byte order: `0`=YUYV, `1`=UYVY, `2`=YYUV, `3`=UVYY        |
| `[4]`     | `enc_en`        | Encoder enable                                                  |
| `[7:5]`   | —               | Reserved                                                        |
| `[15:8]`  | `x_pixel`       | Horizontal size in units of 8 pixels (640 pixels = `80`)        |
| `[16]`    | `enc_size`      | Append the frame size after the end of each JPEG: a zero byte followed by a 32-bit little-endian byte count (5 bytes after the `FF D9` end marker) |
| `[17]`    | `bit_rate_ctrl` | Automatic bit-rate control enable (see below)                    |
| `[20:18]` | `bit_rate_step` | Bit-rate adjustment step, 0–7. A larger value makes the quality change more slowly. No effect while `bit_rate_ctrl` is 0 |
| `[21]`    | `hsync_rev`     | HSYNC polarity invert                                            |
| `[22]`    | `vsync_rev`     | VSYNC polarity invert                                            |
| `[23]`    | —               | Reserved                                                         |
| `[31:24]` | `y_pixel`       | Vertical size in units of 8 pixels (480 pixels = `60`)           |

### `rx_state`

| Bits   | Name         | Purpose                                              |
| ------ | ------------ | ----------------------------------------------------- |
| `[0]`  | `empty_fifo` | `1` when `rx_fifo_data` holds no word                  |
| `[1]`  | —            | Not characterised                                      |
| `[31:2]` | —          | Reserved                                               |

### `status`

| Bits  | Name                 | Purpose                                         |
| ----- | -------------------- | ------------------------------------------------ |
| `[0]` | `start_frm_int_status` | Start-of-frame interrupt pending. Write 1 to clear |
| `[1]` | `end_frm_int_status`   | End-of-frame interrupt pending. Write 1 to clear   |

## Operation model

`div` sets the sensor MCLK output. MCLK runs only while the block's clock gate
is powered and `enc_en` is set; with `enc_en` clear no clock reaches the
sensor. `enc_en` also gates the capture/encode pipeline: PCLK/HSYNC/VSYNC/PXD
sampling, quantization, and JPEG output only run while it is set.

The quantization table occupies its own 32-word RAM region and is not reset by
a write to `ctrl0`/`ctrl1` — it must be loaded once before the first encode
and is otherwise persistent.

Once `enc_en` is set and frames are arriving, the block free-runs: it samples
each incoming frame against HSYNC/VSYNC, encodes it through the quantization
table, and pushes the compressed bytes into `rx_fifo_data` for the DMA (or
CPU) to drain. `byte_cnt_pfrm` reports the size of the most recently completed
frame; `status` flags start/end of each frame for interrupt-driven draining.

## Output stream

Each frame is a complete JPEG file: it starts with the `FF D8` marker and a
header, then the entropy-coded data, then `FF D9`. With `enc_size` set, the
size trailer described above follows. At 640x480 with the loaded quantization
table the header was observed to be exactly 628 bytes, and frames were 12–27
KB depending on the scene (with bit-rate control off).

Words stay in `rx_fifo_data` until they are read; the FIFO is not cleared
between frames, so words left over from an earlier frame come out in front of
the next one. Read it until `rx_state.empty_fifo` is set before starting a
capture.

The `end_frame` flag marks the end of the frame; the last words (`FF D9` and
trailer) can still be waiting in the FIFO when it is raised. The next frame
starts 21–82 µs later,
measured with the sensor running at about 16 frames per second, so a transfer
that has to catch the first word of a frame must be armed within that gap.

DMA drains the FIFO by using it as a peripheral source (request line 8). The
transfer is paced by the FIFO: it waits while `empty_fifo` is set and resumes
when a word arrives, so a transfer sized larger than a frame does not end by
itself at the end of the frame. The bytes received can be read from the DMA
write counter; a transfer stopped after `end_frame` once the FIFO is empty
and the counter has stopped moving holds exactly one frame.

## Bit-rate control

With `bit_rate_ctrl` set, after each frame the encoder compares the frame size
with `target_byte_l` (lower bound) and `target_byte_h` (upper bound), both in
bytes, and changes the quantization for the following frames: coarser when the
frame was above the upper bound, finer when it was below the lower bound.
`bit_rate_step` sets how fast the quality moves. A scene that compresses well
(for example a uniform colour) is driven to maximum quality to reach the lower
bound, so the lower bound acts as a floor on frame size. While `bit_rate_ctrl`
is clear, `bit_rate_step`, `target_byte_h` and `target_byte_l` have no effect.
The bounds are not a hard limit: the controller reacts one or more frames late.

## Sync polarity

`hsync_rev` and `vsync_rev` must match the sensor. With an HI704 sensor,
`hsync_rev = 0` and `vsync_rev = 0` gives a correct picture. With both set to
`1` complete JPEG frames are still produced, but the picture is grey or
corrupt, so a wrong polarity is not detected by any error flag.

## Operation sequence

Minimum sequence to produce a running sensor MCLK (no capture):

1. Power the block's clock gate in the ICU.
2. Configure the DVP pin group (MCLK/PCLK/HSYNC/VSYNC/PXD0-7) into second
   function mode.
3. Write `ctrl0.div` for the desired MCLK rate (`0` selects 24 MHz).
4. Write `ctrl1.enc_en = 1`.

Full capture/encode additionally requires, before step 4:

- Load the quantization table.
- Set `ctrl1.x_pixel`/`y_pixel` to the sensor's frame dimensions divided by 8.
- Configure `yuv_fmt_sel`, `hsync_rev`/`vsync_rev` to match the sensor, and set
  `video_byte_rev` for the byte order wanted from `rx_fifo_data`.
- Optionally enable `enc_size` and bit-rate control (`bit_rate_ctrl`,
  `bit_rate_step`, `target_byte_h`/`target_byte_l`).

## Interrupts

The block's interrupt is wired to the FIQ line of the interrupt controller,
not to IRQ. `ctrl0.start_frm_int` and `ctrl0.end_frm_int` gate the two
frame-boundary interrupts; `status` reports and acknowledges them (write 1 to the
corresponding bit to clear). Both are frame-level only — there is no
per-byte or per-line interrupt; FIFO draining during a frame is expected to
run from DMA.

## Notes

- **A sensor with no MCLK can jam a shared control bus it sits on.** Observed
  on hardware with an HI704 sensor sharing its I2C control bus with other
  traffic: with MCLK not running, every transaction on that I2C bus timed
  out — not just transactions addressed to the sensor — because the
  unclocked sensor held a bus line in an undefined state. The bus returned to
  normal, zero-timeout operation as soon as MCLK started (`div` written and
  `enc_en` set). This is a property of the sensor, not of this block or of
  the I2C controller, but it means MCLK must be running before any bus the
  sensor shares is used, regardless of whether capture/encode is ever
  enabled.
- `div = 3` is redundant with `div = 0` — both select 24 MHz.
- Bit 1 of `rx_state` is not characterised.
