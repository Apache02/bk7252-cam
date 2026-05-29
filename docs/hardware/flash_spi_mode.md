# BK7252 SPI Flash Programming Mode

The BK7252 bootloader ROM includes a pass-through mode that routes the internal
SPI NOR flash bus to external chip pins, allowing an external SPI master to
program the flash directly — bypassing the BK7252 CPU and flash controller
entirely. This mode is entered at power-on by holding the flash CS# pin low
during reset and completing a handshake sequence.

All commands in this mode are standard SPI NOR wire bytes. The BK7252 flash
controller opcode IDs (`op_type_sw` values from `flash.md`) are not used here.

---

## Hardware connections

In SPI flash mode the BK7252 exposes the internal flash SPI bus on the
following chip pins (A9 camera board):

| SPI NOR signal | BK7252 pin | Direction (from external master) |
| -------------- | ---------- | -------------------------------- |
| CLK            | GPIO10     | Output                           |
| DI (MOSI)      | GPIO11     | Output                           |
| DO (MISO)      | GPIO12     | Input                            |
| CS#            | GPIO9      | Output (active low)              |
| CEN            | GPIO17     | Output — chip reset line         |

WP# and HOLD# are not routed to external pins in the known board configuration;
the flash chip hardware WP# mechanism is inactive in this mode (SRP=0 in the
factory-default SR).

---

## Entry sequence

The BK7252 bootloader ROM monitors CS# during the power-on or CEN reset cycle.
If CS# is held low throughout, the ROM enters pass-through mode and connects the
internal flash SPI bus to the external pins.

```
1. Configure SPI: Mode 0 (CPOL=0, CPHA=0) or Mode 3 (CPOL=1, CPHA=1).
   Clock speed: 30–50 kHz confirmed. Higher speeds not characterised.

2. Assert CS# low. Keep it low for the entire handshake.

3. Assert CEN low for ≥ 1 s (chip reset).

4. De-assert CEN (high). The ROM begins executing.

5. Send 250 bytes of 0xD2 on MOSI while reading MISO.
   Expected MISO: first byte = 0xD2, remaining bytes = 0x00.
   A first byte other than 0xD2 means the ROM did not enter pass-through mode;
   de-assert CS#, return to step 2 and retry.

6. Send 32 bytes of 0x00 on MOSI (discard MISO).
   Reason not documented; required for the ROM to complete its initialisation.

7. De-assert CS# (high). The flash chip is now accessible.
```

After a successful handshake, all subsequent SPI transactions follow the
standard SPI NOR protocol with CS# toggled per-transaction.

**Note on step 5 byte count.** The forum source used exactly 250 bytes. The
RP2040 implementation splits this as 16 write-only bytes followed by 250
read bytes (total 266 bytes of 0xD2 on MOSI). Both approaches work on BK7252.
Adjust the count if a specific target ROM revision behaves differently.

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

```
CS# → low
Send: 0x9F 0x00 0x00 0x00
Recv: [ignore] [0x1C] [0x70] [0x15]    ← EN25QH16B
CS# → high
```

If the first non-dummy byte is not `0x1C`, the handshake did not succeed or
the flash chip is different. Re-run the entry sequence.

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
At runtime the BK7252 bootloader sets SR = `0x7C`; clear it before programming.

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

To re-enable the bootloader protection after programming:

```
CS# → low ; Send: 0x06 ; CS# → high                    ← WREN
CS# → low ; Send: 0x01 0x7C ; CS# → high               ← WRSR(0x7C)
Poll WIP until 0.
```

SR = `0x7C` sets 4KBL=1, TB=1, BP2:0=111, protecting sector 0
(`0x000000–0x007FFF`, lower 32 KB). The BK7252 bootloader enforces this
value on every normal boot, so restoring it is optional but matches the
expected runtime state.

### Exit programming mode

Power-cycle the board, or toggle CEN without holding CS# low:

```
CEN → low (≥ 10 ms)
CEN → high
```

The BK7252 ROM will not enter pass-through mode without CS# held low, so a
normal boot follows.

---

## Comparison with OPERATE_SW path

| Property                  | SPI flash mode (external) | OPERATE_SW (internal)            |
| ------------------------- | ------------------------- | -------------------------------- |
| Commands                  | Standard JEDEC wire bytes | BK7252 `op_type_sw` identifiers  |
| PP granularity            | 1–256 bytes per page      | 32 bytes (hardware FIFO limit)   |
| Read granularity          | Arbitrary (streaming)     | 32 bytes per trigger             |
| XIP CRC interleaving      | Not present (raw bytes)   | 34-byte physical blocks on reads |
| BP check enforcement      | Flash chip hardware only  | Flash chip + BK7252 controller   |
| CPU running               | No (ROM pass-through)     | Yes                              |
| Access to OTP sectors     | Yes (before SPL lock)     | Yes (before SPL lock)            |

---

## Notes

- **Handshake token `0xD2`** and the 32-byte post-handshake read are specific
  to the BK7252 ROM. They are not documented in any public Beken datasheet;
  the sequence was reverse-engineered from community sources.

- **SPI clock speed.** 30–50 kHz is confirmed working. The BK7252 ROM likely
  latches data on a software-bit-bang loop; very high speeds may fail. The
  EN25QH16B itself supports up to 83 MHz (READ) at nominal voltage, but the
  ROM is the bottleneck.

- **SR = `0x7C` at runtime.** The BK7252 bootloader ROM writes this value on
  every normal boot. Always clear protection before programming; always confirm
  SR after WRSR by reading it back.

- **CE requires SR = `0x00`.** The flash chip rejects Chip Erase when any BP
  bit is set. Clear SR before issuing CE.

- **OTP sectors.** The top three 4 KB sectors (509–511,
  `0x1FD000–0x1FFFFF`) contain 512-byte OTP-lockable sub-regions. They
  behave as normal flash unless the corresponding SPL bits are set via WRSR
  in OTP mode. Avoid programming SPL bits unless intentional — they are
  one-time-programmable and cannot be cleared.

- **No BK7252 controller BP suppression in this mode.** The BK7252 flash
  controller (which suppresses PP when any BP bit is non-zero regardless of
  target address) is not in the data path. Only the flash chip's own hardware
  protection applies, which correctly gates writes based on address range.

- **Power-up timing.** After CEN goes high, the ROM requires time to start
  before the first SPI byte is clocked. The 1-second CEN-low period covers
  this; do not shorten below ~100 ms.
