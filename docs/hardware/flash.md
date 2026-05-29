# BK7252 Flash Controller

The flash controller at `0x00803000` is a software-controlled SPI master that
programs and erases the on-board SPI NOR flash and exposes its contents to the
CPU through an XIP (execute-in-place) window at `0x00000000–0x001FFFFF`.
The controller contains two independent 32-byte (8-word) FIFOs — one for each
data path (read and write) — a command engine that translates opcode identifiers
to SPI wire sequences, and a hardware CRC checker that validates 34-byte
physical blocks during XIP fetches.

---

## Register map

Offsets are in 32-bit words from base `0x00803000`.

| Offset | Name              | Purpose                                                         |
| ------ | ----------------- | --------------------------------------------------------------- |
| `0x00` | `operate_sw`      | Command trigger: address, opcode, enable, WP, busy              |
| `0x01` | `data_sw_flash`   | Write FIFO: SW→flash, 8 words (one 32-byte block per operation) |
| `0x02` | `data_flash_sw`   | Read FIFO: flash→SW, 8 words (one 32-byte block per operation)  |
| `0x03` | —                 | Reserved                                                        |
| `0x04` | `JEDEC_ID`        | JEDEC ID read by the last RDID command (read-only)              |
| `0x05` | `sr_data_crc_cnt` | SR readback, CRC error counter, FIFO word pointers              |
| `0x06` | —                 | Reserved                                                        |
| `0x07` | `conf`            | Clock, SPI line mode, WRSR data, CPU write gate, XIP CRC       |

### `operate_sw` bits

| Bits     | Name         | Purpose                                                         |
| -------- | ------------ | --------------------------------------------------------------- |
| `[23:0]` | `addr`       | 24-bit byte address passed to the flash chip                    |
| `[28:24]`| `op_type_sw` | Opcode identifier; see opcode table below                       |
| `[29]`   | `op_sw`      | Write 1 to issue the command; self-clearing                     |
| `[30]`   | `wp`         | 1 = de-assert WP# pin; required for write and erase operations  |
| `[31]`   | `busy`       | 1 while an operation is in progress; read-only                  |

### `sr_data_crc_cnt` bits

| Bits     | Name             | Purpose                                                             |
| -------- | ---------------- | ------------------------------------------------------------------- |
| `[7:0]`  | `sr`             | Status register byte returned by the last RDSR or RDSR2 command    |
| `[15:8]` | `error_count`    | Not investigated; never observed to increment                       |
| `[18:16]`| `rd_fifo_ptr`    | Read FIFO word pointer: auto-increments on each offset-2 read; read-only (writes ignored, confirmed on hardware); reset to 0 by READ command |
| `[21:19]`| `wr_fifo_ptr`    | Write FIFO status; reads `7` constantly regardless of writes to offset-1; read-only (writes ignored, confirmed on hardware) |
| `[29:22]`| `m_value`        | Mode byte for quad continuous-read; set per flash chip config       |
| `[30]`   | `page_write_en`  | Not investigated; sticky on BK7221U but function unknown; SDK uses only on BK7238/BK7252N |

### `conf` bits

| Bits     | Name          | Purpose                                                                |
| -------- | ------------- | ---------------------------------------------------------------------- |
| `[3:0]`  | `clk_conf`    | Flash SPI clock selection; `5` = 60 MHz (DPLL source)                  |
| `[8:4]`  | `model_sel`   | SPI line mode for reads: `0` = single, `1` = dual, `2` = quad         |
| `[9]`    | `cpu_data_wr` | Not investigated (SDK name: `FWREN_FLASH_CPU`)                         |
| `[25:10]`| `wrsr_data`   | 16-bit payload written to the flash SR by WRSR or WRSR2                |
| `[26]`   | `crc_en`      | 0 = disable XIP CRC validation (no abort, address mapping unchanged); 1 = enforce CRC (abort on mismatch) |

### Opcode table

The `op_type_sw` field selects the SPI command the controller issues.
The controller internally maps these identifiers to chip-specific SPI wire bytes.

| `op_type_sw` | Name    | Operation                                                     |
| ------------ | ------- | ------------------------------------------------------------- |
| `1`          | `WREN`  | Set Write Enable Latch in flash SR                            |
| `2`          | `WRDI`  | Clear Write Enable Latch                                      |
| `3`          | `RDSR`  | Read flash SR1 → `sr_data_crc_cnt.sr`                        |
| `4`          | `WRSR`  | Write flash SR1 with `conf.wrsr_data[7:0]`                   |
| `5`          | `READ`  | Read 32 bytes → `data_flash_sw` FIFO                         |
| `6`          | `RDSR2` | Read flash SR2 → `sr_data_crc_cnt.sr`                        |
| `7`          | `WRSR2` | Write flash SR1+SR2 with `conf.wrsr_data[15:0]`              |
| `12`         | `PP`    | Page Program: 32 bytes from `data_sw_flash` FIFO             |
| `13`         | `SE`    | Sector Erase: 4 KB sector containing `addr`                  |
| `14`         | `BE1`   | Block Erase: 32 KB block containing `addr`                   |
| `15`         | `BE2`   | Block Erase: 64 KB block containing `addr`                   |
| `16`         | `CE`    | Chip Erase                                                    |
| `17`         | `DP`    | Deep Power Down; `addr` ignored                               |
| `18`         | `RFDP`  | Release from Deep Power Down; `addr` ignored                  |
| `20`         | `RDID`  | Read JEDEC ID → `JEDEC_ID` register                          |
| `21`         | `HPM`   | High Performance Mode                                         |
| `22`         | `CRMR`  | Continuous Read Mode Reset                                    |
| `23`         | `CRMR2` | Continuous Read Mode Reset (variant 2)                        |

---

## Operation model

### OPERATE_SW path

Software commands the controller by writing a complete `operate_sw` value with
`op_sw = 1` to trigger the operation, then polling `busy` until it clears.
All READ and PP operations transfer exactly one 32-byte aligned block: READ
fills the 8-word `data_flash_sw` FIFO; PP drains the 8-word `data_sw_flash`
FIFO. Unaligned access is not supported in hardware — the caller must align
addresses and handle head/tail bytes in software.

### XIP path

CPU instruction and data fetches bypass the OPERATE_SW engine entirely. In XIP
mode the physical flash is interleaved: every 32 logical bytes occupy 34
physical bytes (32 data + 2-byte CRC16-CCITT, polynomial `0x8005`, initial
value `0xFFFF`, big-endian). The controller strips the CRC bytes on each fetch.

**Address mapping.** XIP logical address `L` and physical flash byte offset `P`
are related by the interleaving:

```
logical  → physical:  P = (L / 32) * 34 + (L % 32)
physical → logical:   L = (P / 34) * 32 + (P % 34)   // valid only when P % 34 < 32
```

Both directions confirmed on hardware (EN25QH16B, CRC16 poly `0x8005`, init `0xFFFF`).

**CRC validation is enforced: mismatch causes a Data Abort, not silent
bad-data return.** A CPU fetch whose 34-byte physical block carries an invalid
stored CRC (including blocks written raw via OPERATE_SW without appending CRC
bytes) triggers a Data Abort exception immediately.

**Effective XIP range.** A 2 MiB flash (`0x000000–0x1FFFFF`) contains 61 680
complete 34-byte blocks (`61680 × 34 = 0x1FFF60`). The last valid XIP logical
address is `0x1E1BFF` (block 61 679, last byte). Accesses at `0x1E1C00` and
above map to physical positions where the CRC bytes fall outside the 2 MiB
window and the controller triggers a Data Abort.

**The OPERATE_SW path sees raw physical bytes.** The 2 CRC bytes per 34-byte
block are visible as ordinary data on reads and must be accounted for in writes
targeting the XIP region.

### Write protection model

Two independent protection mechanisms must both be satisfied for PP and SE to
succeed:

1. **Flash SR Write Enable Latch (WEL).** The flash chip requires a WREN
   command before every PP, SE, or WRSR. WEL is cleared by the flash chip
   after each such operation completes — including after a rejected attempt.
   **The controller does not issue WREN automatically. Every erase or program
   sequence must begin with an explicit WREN operation.**

2. **Controller BP check.** The controller reads the flash SR block-protection
   (BP) bits before issuing PP. **If any BP bit is non-zero the controller
   suppresses PP entirely — the command is not put on the SPI bus and WEL is
   not consumed — regardless of whether the target address falls within the
   flash chip's actual protected range.** SE behavior under active BP
   protection is uncharacterised. Clear protection via WREN + WRSR before
   erasing or programming.

### Flash chip status register

The `RDSR` opcode returns the flash chip SR byte into `sr_data_crc_cnt.sr`.
The `WRSR` opcode writes `conf.wrsr_data[7:0]` to the chip SR.
Bit layout for EN25QH16B (confirmed on hardware; other flash chips may differ):

| SR bit | Name  | Purpose                                                                    |
| ------ | ----- | -------------------------------------------------------------------------- |
| `[7]`  | SRP   | Status Register Protect: 1 = WRSR blocked when WP# is low                 |
| `[6]`  | 4KBL  | Boot-lock granularity: 0 = 64 KB block (default), 1 = 4 KB sector         |
| `[5]`  | TB    | Top/Bottom protect: 0 = upper flash (default), 1 = lower flash             |
| `[4]`  | BP2   | Block protect bit 2 (see protected-area table in flash chip datasheet)     |
| `[3]`  | BP1   | Block protect bit 1                                                        |
| `[2]`  | BP0   | Block protect bit 0                                                        |
| `[1]`  | WEL   | Write Enable Latch; read-only                                              |
| `[0]`  | WIP   | Write In Progress; read-only                                               |

The EN25QH16B ships from the factory with SR = `0x00` (all bits clear, no
protection, memory fully erased to `0xFF`). The SR value `0x7C` (`0111 1100`,
4KBL=1, TB=1, BP2:0=`111`) observed at runtime is written by the BK7252
bootloader ROM during its initialisation to protect the lower 32 KB
(`0x000000–0x007FFF`, sector 0) that holds the bootloader itself.

Because the BK7252 controller BP check is address-agnostic, PP to any address
(including sectors outside the protected range) is suppressed while any BP bit
is set. Issue WREN + WRSR(`0x00`) to clear all protection before programming.

---

## Operation sequences

### Read 32 bytes

1. Poll `operate_sw.busy` until 0.
2. Write `operate_sw` atomically:
   `(aligned_addr & 0xFFFFFF) | (READ << 24) | (1 << 29) | (1 << 30)`.
3. Poll `operate_sw.busy` until 0.
4. Read 8 words from `data_flash_sw`; `words[0]` holds bytes at `aligned_addr+0..+3`.

`aligned_addr` must satisfy `addr & 0x1F == 0`.

### Erase 4 KB sector

1. Poll `operate_sw.busy` until 0.
2. Issue WREN: write `operate_sw = (WREN << 24) | (1 << 29) | (1 << 30)`.
   Poll `busy` until 0.
3. Write `operate_sw` atomically:
   `(addr & 0xFFFFF000) | (SE << 24) | (1 << 29) | (1 << 30)`.
4. Poll `operate_sw.busy` until 0.

The controller aligns `addr` down to the nearest 4 KB boundary internally.
Sector erase takes up to 300 ms (t_SE max at 2.7–3.6 V on EN25QH16B).

### Program 32 bytes

The target sector must be in the erased state (all `0xFF`). PP can only clear
bits (write 1→0); erased bits that fall outside the written range survive
unmodified only if a read-modify-write is used.

1. Issue WREN: write `operate_sw = (WREN << 24) | (1 << 29) | (1 << 30)`.
   Poll `busy` until 0.
2. Write 8 words to `data_sw_flash`; `words[0]` holds bytes for
   `aligned_addr+0..+3`.
3. Write `operate_sw` atomically:
   `(aligned_addr & 0xFFFFFF) | (PP << 24) | (1 << 29) | (1 << 30)`.
4. Poll `operate_sw.busy` until 0. Page program completes in up to 3 ms
   (t_PP); polling `busy` covers the wait.

`aligned_addr` must satisfy `addr & 0x1F == 0`.

### Clear write protection

1. Issue WREN: write `operate_sw = (WREN << 24) | (1 << 29) | (1 << 30)`.
   Poll `busy` until 0.
2. Write the desired SR value into `conf.wrsr_data` (bits `[25:10]`).
3. Write `operate_sw = (WRSR << 24) | (1 << 29) | (1 << 30)`.
   Poll `busy` until 0.

To remove all block protection, write `0x00` to `conf.wrsr_data` before step 3.
Use WRSR2 (opcode 7) to write a 16-bit value covering SR1+SR2 on chips with
two status register bytes. After the WRSR trigger, poll `operate_sw.busy` until
0; the internal SR write cycle completes in up to 30 ms (t_W max on EN25QH16B).

### Read JEDEC ID

1. Poll `operate_sw.busy` until 0.
2. Write `operate_sw = (RDID << 24) | (1 << 29) | (1 << 30)`.
   Poll `busy` until 0.
3. Read `JEDEC_ID` register.

### Read status register

1. Poll `operate_sw.busy` until 0.
2. Write `operate_sw = (RDSR << 24) | (1 << 29) | (1 << 30)`.
   Poll `busy` until 0.
3. Read `sr_data_crc_cnt.sr` (bits `[7:0]`).

Use RDSR2 (opcode 6) for SR2 on chips with two status register bytes.

---

## Notes

- **Write `operate_sw` atomically.** `op_sw` triggers immediately when written.
  A read-modify-write sequence that touches `addr` or `op_type_sw` while
  `op_sw` is 1 re-triggers the previous operation with stale state. Compose the
  full 32-bit value and write it in one store.

- **`op_sw` is self-clearing.** Hardware clears it when the operation
  completes. Do not clear it manually.

- **`wp` (bit 30) controls the WP# pin only.** Setting `wp = 1` de-asserts
  the physical WP# line. It does not issue WREN and does not bypass SR write
  protection. Both `wp = 1` and an explicit prior WREN are required for PP and
  SE to proceed.

- **`cpu_data_wr` (conf bit 9) — not investigated.** SDK name is
  `FWREN_FLASH_CPU`; no SDK driver uses it. Exact function unknown.

- **`crc_en` (conf bit 26) disables CRC validation only; address mapping is
  always CRC-interleaved regardless of this bit.** With `crc_en=0`, XIP reads
  beyond the physical flash boundary return garbage instead of causing a Data
  Abort. The CRC-interleaved address formula `P = (L/32)*34 + (L%32)` applies
  in both modes.

- **XIP CRC mismatch causes a Data Abort when `crc_en=1`.** The controller
  does not return bad data silently. `sr_data_crc_cnt.error_count` did not
  increment in any tested scenario (0 with valid CRC, 0 with `crc_en=0`); with
  `crc_en=1` and invalid CRC the chip resets via Data Abort before the register
  can be read. Function of `error_count` not confirmed.

- **OPERATE_SW reads from the XIP region return raw physical bytes.** The 2
  CRC bytes following each 32-byte data group are returned as data. Physical
  byte offset for logical offset `n` is `(n / 32) * 34 + (n % 32)`. To write
  data into the XIP region that can subsequently be fetched via XIP, CRC bytes
  must be computed (CRC16, poly `0x8005`, init `0xFFFF`) and written to the
  correct physical positions; raw writes without CRC will cause Data Abort on
  any XIP fetch of those blocks.

- **BE1, BE2** follow the same WREN-required pattern as SE. Controller BP
  suppression behaviour for these opcodes under active SR protection is not
  characterised.

- **CE (Chip Erase) is rejected by the flash chip itself** when any BP bit is
  set — this is a flash-chip-level constraint, not only a BK7252 controller
  check. Clear SR before issuing CE.

- **DP / RFDP**: after RFDP, wait at least t_RES1 before issuing any further
  command. On EN25QH16B: t_RES1 max = 3 µs (release without Device ID read);
  t_RES2 max = 1.8 µs (release with Device ID read via `ABh`).

- **CRMR / CRMR2** reset continuous-read mode on chips that use the M-byte
  mechanism for quad I/O. Issue before switching `model_sel` from quad to a
  lower mode.

- **`m_value` (sr_data_crc_cnt bits `[29:22]`)**: the M-byte transmitted during
  quad continuous-read (QPI). Set per flash chip datasheet before enabling
  `model_sel = 2`. Not required for single or dual SPI.

- **`model_sel`** affects the SPI read command issued by the XIP engine; it does
  not affect OPERATE_SW READ or PP operations, which always use standard
  single-wire SPI.

- **SPI clock selection (`conf.clk_conf`)**: value `5` selects 60 MHz from the
  DPLL source. Other values are not characterised; leave at the reset default
  until the flash chip's maximum frequency is confirmed.

- **OTP security sectors.** EN25QH16B reserves the first 512 bytes of each of
  the top three 4 KB sectors as OTP-lockable regions (sector 511:
  `0x1FF000–0x1FF1FF`; 510: `0x1FE000–0x1FE1FF`; 509: `0x1FD000–0x1FD1FF`).
  They behave as normal flash until the corresponding SPL bit is set via WRSR
  in OTP mode (opcode `0x3A`). Once an SPL bit is set it cannot be cleared.
  The OPERATE_SW path does not expose the OTP-mode entry opcode.

- **Volatile WRSR (flash chip opcode `0x50`).** EN25QH16B supports a Volatile
  Status Register Write Enable command that allows writing SR without consuming
  non-volatile bit endurance — useful for temporarily clearing BP bits. The
  change is lost on power-cycle or software reset. The BK7252 controller does
  not expose this opcode; use WREN (`0x06`) + WRSR (`0x01`) for all SR writes
  through the OPERATE_SW path.
