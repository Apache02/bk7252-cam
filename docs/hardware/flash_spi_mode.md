# BK7252 SPI Flash Programming Mode

The BK7252 implements a SPI Flash Programming Mode that exposes direct SPI NOR
access to the internal flash memory through external chip pins. The mechanism
remains available even when the flash contents are erased, allowing recovery of
a blank device.

After a reset, the chip opens a short entry window during which a 0xD2
handshake can activate the programming mode. Once activated, standard SPI NOR
commands can be issued directly to the flash memory.

---

## Hardware connections

In SPI flash mode the BK7252 exposes the internal flash SPI bus on the
following chip pins (A9 camera board):

| Test point | BK7252 GPIO | SPI NOR signal | Direction (from external master) |
| ---------- | ----------- | -------------- | -------------------------------- |
| TCK        | GPIO20      | CLK            | Output                           |
| TDI        | GPIO22      | DI (MOSI)      | Output                           |
| TDO        | GPIO23      | DO (MISO)      | Input                            |
| TMS        | GPIO21      | CS#            | Output (active low)              |
| CEN        | —           | CEN            | Output (active low)              |

WP# and HOLD# are not routed to external pins in the known board configuration;
the flash chip hardware WP# mechanism is inactive in this mode (SRP=0 in the
factory-default SR).

---

## Entry sequence

After CEN is released the BK7252 opens a ~53 ms SPI Flash Programming Mode
entry window. During this window 0xD2 is driven continuously on MISO regardless
of CS# state. Entry is triggered by receiving 0xD2 bytes on MOSI within this
window while CS# is held low. CS# does **not** need to be asserted before or
during the CEN reset pulse.

```
1. Configure SPI: Mode 0 (CPOL=0, CPHA=0). Mode 3 does not work.
   Clock speed: 30–50 kHz confirmed.

2. De-assert CS# (high).

3. Assert CEN low for ≥ 100 ms (chip reset).

4. De-assert CEN (high).

5. Assert CS# low.

6. Wait ≥ 5 ms. This positions the transaction inside the ~53 ms entry window.

7. Send 2 bytes of 0xD2 on MOSI while reading MISO.
   Expected MISO[0] = 0xD2.
   MISO[1] is typically observed as 0x00 but is not used as part of the
   success criterion.
   If MISO[0] ≠ 0xD2 the handshake failed; de-assert CS#, return to step 2.

8. De-assert CS# (high). This separates the handshake transaction from the
   subsequent SPI NOR transactions.

9. Wait ≥ 1 ms.

10. Assert CS# low.

11. Read 2 bytes with MOSI = 0x00 (discard). These bytes are discarded. They
    contain residual output produced immediately after the handshake and do not
    belong to a normal SPI NOR transaction.

12. Poll: send one byte of 0x9F on MOSI and read MISO.
    Continue until the received byte is neither 0x00 nor 0xD2.
    The appearance of any other value indicates that the chip has switched into
    SPI Flash Programming Mode and is returning flash-generated data.

13. De-assert CS# (high). SPI Flash Programming Mode is now active.
```

After a successful handshake, all subsequent SPI transactions follow the
standard SPI NOR protocol with CS# toggled per-transaction.

**D2 window timing.** The window lasts ~53 ms from CEN↑ at both 30 kHz and
50 kHz, confirmed by measuring the byte count of continuous 0xD2 output on MISO.
The window duration appears to be time-based and was observed to be independent
of SPI clock rate and CS# transitions.

**Step 12 poll duration.** The poll must run for long enough to cover the flash
initialisation delay, which can reach ~40 ms. At 50 kHz each byte takes 160 µs,
so 200 iterations ≈ 32 ms — marginal. At 1 MHz each byte takes 8 µs, so 200
iterations ≈ 1.6 ms — insufficient. Use a time-based timeout (≥ 50 ms) rather
than a fixed iteration count.

---

## SPI NOR command set

Commands are standard JEDEC SPI NOR wire bytes. The EN25QH16B flash chip
(`JEDEC ID 0x1C 0x70 0x15`) supports the following commands relevant to
programming:

| Wire byte | Name   | Payload                                         |
| --------- | ------ | ----------------------------------------------- |
| `0x06`    | WREN   | None. Sets WEL in SR.                           |
| `0x04`    | WRDI   | None. Clears WEL in SR.                         |
| `0x05`    | RDSR   | Returns SR byte continuously until CS# high.    |
| `0x01`    | WRSR   | 1 data byte: new SR value (SR7–SR0).            |
| `0x9F`    | RDID   | Returns 3 bytes: Manufacturer, MemType, MemCap. |
| `0x03`    | READ   | 3-byte address + data bytes (continuous).       |
| `0x02`    | PP     | 3-byte address + 1–256 data bytes.              |
| `0x20`    | SE     | 3-byte address. Erases 4 KB sector.             |
| `0x52`    | HBE    | 3-byte address. Erases 32 KB block.             |
| `0xD8`    | BE     | 3-byte address. Erases 64 KB block.             |
| `0xC7`    | CE     | None. Erases entire chip (requires SR=0x00).    |
| `0xB9`    | DP     | None. Enters Deep Power-down.                   |
| `0xAB`    | RES    | 3 dummy bytes + returns Device ID byte.         |

All write and erase commands require a preceding WREN. WEL is cleared by the
flash chip after each write or erase completes (including rejected attempts).

---

## Operation sequences

### Verify entry / read JEDEC ID

The polling stage in step 12 only detects the transition into SPI Flash
Programming Mode. After the transition is detected, perform a fresh RDID
transaction to verify communication with the flash device:

```
CS# → low
Send: 0x9F 0x00 0x00 0x00
Recv: [ignore] [0x1C] [0x70] [0x15]    ← EN25QH16B
CS# → high
```

If the response bytes are all 0x00, the mode transition has not yet completed;
repeat the transaction. If the first non-dummy byte is not `0x1C`, the
handshake did not succeed or the flash chip is different — re-run the entry
sequence.

### Poll busy (WIP bit)

The Write In Progress (WIP) bit is SR[0]. Poll after every write or erase:

```
CS# → low
Send: 0x05 0x00 0x00 ...    (repeat 0x00 until WIP = 0)
Recv: [ignore] [SR] [SR] ...
CS# → high when SR[0] == 0
```

### Clear write protection

Required before any write or erase if SR BP bits are non-zero.
On tested devices SR = `0x7C` is observed after a normal boot; clear it before
programming.

```
CS# → low ; Send: 0x06 ; CS# → high          ← WREN
Poll WIP until 0.
CS# → low ; Send: 0x01 0x00 ; CS# → high     ← WRSR(0x00)
Poll WIP until 0 (completes in up to 30 ms).
Read SR and confirm SR[4:2] == 0 before proceeding.
```

### Erase 4 KB sector

```
CS# → low ; Send: 0x06 ; CS# → high                    ← WREN
CS# → low ; Send: 0x20 A23 A15 A7 ; CS# → high         ← SE
Poll WIP until 0 (up to 300 ms at 2.7–3.6 V).
```

Any byte address within the target sector is valid. The chip aligns down to
the nearest 4 KB boundary.

### Program 1–256 bytes (Page Program)

The target sector must be in the erased state (`0xFF`). PP can only clear bits
(1→0); writing to a non-erased byte produces an AND of the existing and new
values. PP wraps at a 256-byte page boundary: bytes that overflow the current
page are written from the start of the same page, overwriting earlier bytes.

```
CS# → low ; Send: 0x06 ; CS# → high                    ← WREN
CS# → low
Send: 0x02 A23 A15 A7 D0 D1 ... D(n-1)                 ← PP
CS# → high
Poll WIP until 0 (up to 3 ms at 2.7–3.6 V).
```

To program a full image iterate in 256-byte pages. Erase each 4 KB sector
before programming its pages.

### Read data (arbitrary length)

```
CS# → low
Send: 0x03 A23 A15 A7
Recv: D0 D1 D2 ...    (clock out 0x00 bytes; address auto-increments)
CS# → high
```

READ auto-increments the address. The entire 2 MB flash can be read in a
single transaction.

### Restore write protection

To restore the write protection state observed after a normal boot:

```
CS# → low ; Send: 0x06 ; CS# → high                    ← WREN
CS# → low ; Send: 0x01 0x7C ; CS# → high               ← WRSR(0x7C)
Poll WIP until 0.
```

SR = `0x7C` sets 4KBL=1, TB=1, BP2:0=111, protecting sector 0
(`0x000000–0x007FFF`, lower 32 KB).

### Exit programming mode

Power-cycle the board, or toggle CEN without asserting CS# low:

```
CEN → low (≥ 10 ms)
CEN → high
```

Without the 0xD2 handshake the entry window closes and a normal boot follows.

---

## Comparison with OPERATE_SW path

| Property                  | SPI flash mode (external)          | OPERATE_SW (internal)            |
| ------------------------- | ---------------------------------- | -------------------------------- |
| Commands                  | Standard JEDEC wire bytes          | BK7252 `op_type_sw` identifiers  |
| PP granularity            | 1–256 bytes per page               | 32 bytes (hardware FIFO limit)   |
| Read granularity          | Arbitrary (streaming)              | 32 bytes per trigger             |
| XIP CRC interleaving      | Not present (raw bytes)            | 34-byte physical blocks on reads |
| BP check enforcement      | Flash chip hardware only           | Flash chip + BK7252 controller   |
| CPU running               | Not observable externally          | Yes                              |
| Access to OTP sectors     | Yes (before SPL lock)              | Yes (before SPL lock)            |

---

## Notes

- **Handshake token `0xD2`** is specific to this chip and is not documented in
  any public Beken datasheet. The entry sequence was reverse-engineered from
  community sources and verified by hardware measurement on an A9 camera board
  (RP2040 SPI master at 30–50 kHz).

- **SPI clock speed.** 10 kHz–1 MHz confirmed working. At 1 MHz the step 12
  poll loop must use a time-based timeout (≥ 50 ms) rather than a fixed
  iteration count, otherwise it exits before the flash initialisation completes.
  Speeds above 1 MHz have not been characterised. The EN25QH16B itself supports
  up to 83 MHz (READ) at nominal voltage.

- **SR = `0x7C` after normal boot.** On tested devices SR reads as `0x7C` after
  a normal boot. Always clear protection before programming; always confirm SR
  after WRSR by reading it back.

- **CE requires SR = `0x00`.** The flash chip rejects Chip Erase when any BP
  bit is set. Clear SR before issuing CE.

- **OTP sectors.** The top three 4 KB sectors (509–511,
  `0x1FD000–0x1FFFFF`) contain 512-byte OTP-lockable sub-regions. They
  behave as normal flash unless the corresponding SPL bits are set via WRSR
  in OTP mode. Avoid programming SPL bits unless intentional — they are
  one-time-programmable and cannot be cleared.

- **BP suppression in OPERATE_SW mode.** When using the OPERATE_SW path the
  BK7252 flash controller suppresses PP when any BP bit is non-zero regardless
  of target address. This suppression is not observed in SPI Flash Programming
  Mode; only the flash chip's own hardware protection applies.

- **CEN pulse duration.** 100 ms is confirmed sufficient. The original community
  source specified ≥ 1 s. Do not shorten below ~100 ms.

- **CS# during CEN pulse.** CS# does not need to be held low during the CEN
  reset cycle. The handshake window opens after CEN↑; asserting CS# at any
  point before the window closes is sufficient.
