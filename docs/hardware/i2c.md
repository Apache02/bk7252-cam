# BK7252 Hardware I2C

Two independent single-master controllers, I2C1 (`0x00802300`) and I2C2
(`0x00802600`). They are not variants of the same design — different register
layout, different capabilities — and are documented separately below.

## I2C1

A minimal SMBus/I2C-style controller: one control/status register that also
issues bus commands, and one data register for the byte currently being
shifted in or out. No FIFO — every byte is a separate write/poll/write cycle.

The bus pins (GPIO20 = SCL, GPIO21 = SDA) are configured through the GPIO
block's second-function mux, not through this block.

### Operation model

The controller is a byte-at-a-time state machine driven entirely through `si`.
Every time it finishes a bus phase — the address byte, a data byte, an
ACK/NACK sample — it sets `si` and stops driving the bus until software
services it and clears `si` back to 0.

**`sta` and `sto` are one-shot commands, not status flags. Hardware does not
self-clear them.** Writing 1 to `sta` starts the START condition; the bit
stays 1 in the register until software writes it back to 0. The same is true
of `sto`. This matters because the register is written as a whole word on
every update — a naive read-modify-write that sets `sto = 1` without also
clearing a still-set `sta` from an earlier step leaves both bits 1 in the
same write. **With `sta` and `sto` both 1, the controller does not generate a
clean STOP condition** — the bus instead chains directly into a new START,
with no data byte transmitted, indistinguishable on the wire from repeated
bus noise.

`si` is a plain read/write flag, not a write-1-to-clear register. Software
clears it by writing 0 back into the whole `config` word — which means every
`si`-clearing write is also the point where `sta` must already be 0, or the
corruption above happens silently.

`ack_tx` decides the ACK/NACK bit for the byte the controller just latched
into `data`, not for a byte still to come — set it in the same write that
clears `si` for that byte.

A transaction's byte sequence:

1. Address phase — the driver writes the address+direction byte into `data`,
   then issues `sta`. The controller drives START, clocks the address byte
   out, samples the ACK, and sets `si`. `ack_rx` reflects whether the address
   was acknowledged.
2. Data phase — for each byte:
   - Transmit: software writes the next byte into `data` before clearing `si`.
     The controller clocks it out and sets `si` again once the ACK is sampled.
   - Receive: the controller has already shifted a byte into `data` and set
     `si`; software reads `data`, sets `ack_tx` to ACK that byte if more
     bytes are still wanted or NACK it if it's the last one, and clears `si`
     in the same write.
3. Termination — software issues `sto` (clearing `sta` and `si` in the same
   write, see above). `busy` stays 1 until the STOP condition has actually
   completed on the wire.

`ack_req` is set whenever the controller is waiting on software to supply an
ACK/NACK decision (`ack_tx`) before it can continue a receive; it does not
appear during a transmit-direction transaction.

### Operation sequence

Write (address phase, then N data bytes):

1. Write the address+direction byte (`addr << 1 | 0`) into `data`.
2. Write `config` with `sta = 1`, `tx_mode = 1`, `ensmb = 1` (preserve
   `freq_div`).
3. Poll `si` until it reads 1.
4. Read `ack_rx`. If 0 (NACK), go to step 8.
5. Write the next data byte into `data`.
6. Write `config` with `sta = 0`, `si = 0` (preserve `freq_div`, `ensmb`).
7. Poll `si` until it reads 1; read `ack_rx`; repeat from step 5 for
   additional bytes.
8. Write `config` with `sta = 0`, `si = 0`, `sto = 1` (preserve `freq_div`,
   `ensmb`). Poll `busy` until it reads 0.

Read is identical through the address phase, except the address byte's
direction bit is 1 and `tx_mode` is cleared once the address is acknowledged.
Each subsequent `si` marks one received byte available in `data`. In the same
write that clears `si`: set `ack_tx = 1` to ACK the byte just read and
request another, or `ack_tx = 0` to NACK it and terminate the read (per the
I2C spec, the master NACKs only the last byte it wants, then STOPs).

### Register map

| Offset (bytes) | Name     | Purpose                                    |
| --------------- | -------- | ------------------------------------------ |
| `0x00`           | `config` | Bus commands, status, and clock divider    |
| `0x04`           | `data`   | Byte to transmit / byte last received      |

#### `config`

| Bits      | Name       | Purpose                                                        |
| --------- | ---------- | ---------------------------------------------------------------|
| `[0]`     | `ensmb`    | 1 = engine enabled                                              |
| `[1]`     | `sta`      | Write 1 to issue a START condition                              |
| `[2]`     | `sto`      | Write 1 to issue a STOP condition                                |
| `[3]`     | `ack_tx`   | ACK/NACK to send for the byte just received (read transfers only) |
| `[4]`     | `tx_mode`  | 1 = transmit, 0 = receive                                       |
| `[5]`     | —          | Reserved                                                        |
| `[15:6]`  | `freq_div` | Bus clock divider, 10 bits (0–1023)                              |
| `[16]`    | `si`       | Serial interrupt flag — set by hardware on every state-machine step, cleared by software |
| `[17]`    | `ack_rx`   | ACK/NACK bit of the last byte transferred. Read-only.           |
| `[18]`    | `ack_req`  | Set once the controller needs an ACK/NACK decision from software. Read-only. |
| `[19]`    | `busy`     | Bus busy. Read-only.                                             |
| `[31:20]` | —          | Reserved                                                         |

#### `data`

| Bits     | Name   | Purpose                                       |
| -------- | ------ | ---------------------------------------------- |
| `[7:0]`  | `data` | Next byte to transmit / last byte received     |
| `[31:8]` | —      | Reserved                                       |

### Interrupts

`si` also drives the I2C1 line into the interrupt controller (ICU bit 2 in the
IRQ source map). With that source enabled in the ICU's `irq_enable`, every
`si` assertion raises an interrupt; the handler runs the same state machine
described above instead of a polling loop. `si` itself is still cleared by
the handler exactly as in the polling case — the ICU-level status/ack
registers are a separate, outer layer and do not substitute for clearing the
peripheral's own `si` bit.

### Notes

- **`sta`/`sto` are not self-clearing, and both being 1 at once produces no
  STOP condition on the wire** — see Operation model. This is the single most
  consequential detail of this block; every register write from address phase
  onward must explicitly account for the current state of `sta`.
- `freq_div` sets the bus clock: `scl_freq ≈ ref_clk / (6 + 3 × (freq_div + 1))`,
  where `ref_clk` is whichever source the ICU clock mux selects for I2C1 (26 MHz
  XTAL or DCO). At 26 MHz, `freq_div = 84` gives ~100 kHz; `freq_div = 1023`
  (the field's maximum) gives the slowest rate the register can express,
  ~8.4 kHz. There is no separate clock-enable bit beyond `ensmb`.
- `ensmb`, `freq_div`, `sta`, `sto`, `si` all live in the same 32-bit word.
  Any write to one must be a read-modify-write that preserves the others, or
  the write silently resets `freq_div` and drops `ensmb` alongside whatever
  bit was intended.
- `ack_req` and `ack_rx` are read-only; there is no way to force an ACK on the
  wire for a transmit-direction byte — the addressed device controls it.

## I2C2

A different, more capable block: slave-mode address matching, arbitration-loss
detection, SCL timeout detection, and a small FIFO (`rxfifo_empty`,
`txfifo_full`). Register layout below is confirmed against the vendor header;
the operation model and sequence are not yet characterised — no driver exists
for this block yet.

### Register map

| Offset (bytes) | Name     | Purpose                                    |
| --------------- | -------- | ------------------------------------------ |
| `0x00`           | `config` | Bus config, slave address, clock divider   |
| `0x04`           | `status` | Bus commands and status                    |
| `0x08`           | `data`   | Byte to transmit / byte last received      |

#### `config`

| Bits      | Name       | Purpose                                   |
| --------- | ---------- | ------------------------------------------ |
| `[2:0]`   | `idle_cr`  | Idle-bus timing config                     |
| `[5:3]`   | `scl_cr`   | SCL timing config                          |
| `[15:6]`  | `freq_div` | Bus clock divider, 10 bits                 |
| `[25:16]` | `slv_addr` | Own slave address (slave mode)             |
| `[27:26]` | `smb_cs`   | SMBus clock select                         |
| `[28]`    | `toe`      | Timeout enable                             |
| `[29]`    | `fte`      | FIFO/transfer enable                       |
| `[30]`    | `inh`      | Inhibit                                    |
| `[31]`    | `ensmb`    | 1 = engine enabled                         |

#### `status`

| Bits      | Name           | Purpose                                        |
| --------- | -------------- | ------------------------------------------------|
| `[0]`     | `si`           | Serial interrupt flag; clear by writing 0        |
| `[1]`     | `scl_timeout`  | SCL timeout occurred                             |
| `[2]`     | —              | Reserved                                         |
| `[3]`     | `arb_lost`     | Arbitration lost                                 |
| `[4]`     | `rxfifo_empty` | RX FIFO empty. Read-only.                        |
| `[5]`     | `txfifo_full`  | TX FIFO full. Read-only.                         |
| `[7:6]`   | `int_mode`     | Interrupt mode                                   |
| `[8]`     | `smbus_ack`    | ACK bit                                          |
| `[9]`     | `smbus_stop`   | Generate/detect STOP                             |
| `[10]`    | `smbus_sta`    | Generate/detect START                            |
| `[11]`    | `addr_match`   | Address matched (slave mode). Read-only.         |
| `[12]`    | `ack_req`      | ACK/NACK decision needed. Read-only.             |
| `[13]`    | `tx_mode`      | 1 = transmit, 0 = receive                        |
| `[14]`    | `master`       | Master mode active. Read-only.                   |
| `[15]`    | `busy`         | Bus busy. Read-only.                             |
| `[31:16]` | —              | Reserved                                         |

#### `data`

| Bits     | Name   | Purpose                                       |
| -------- | ------ | ---------------------------------------------- |
| `[7:0]`  | `data` | Next byte to transmit / last byte received     |
| `[31:8]` | —      | Reserved                                       |

### Operation model / sequence

Not characterised yet — needs the same hardware bring-up I2C1 got (a working
driver, verified against a sniffer) before this section can be written.
