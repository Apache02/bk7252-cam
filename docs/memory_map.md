# BK7252 Memory Map

ARM968E-S, 32-bit address space. All addresses are byte addresses; the bus is
little-endian. Flash appears twice (XIP window + mirror); RAM appears in two
physically separate blocks.

---

## Address Space Summary

| Range                       | Size   | Region                              |
|-----------------------------|--------|-------------------------------------|
| `0x00000000`–`0x0000FFFF`   | 64 KiB   | Flash XIP — bootloader region       |
| `0x00010000`–`0x001FFFFF`   | ~2 MiB   | Flash XIP — app / OTA region (see `docs/partitions.md`) |
| `0x00200000`–`0x003F7FFF`   | ~2 MiB  | Flash upper window: 4 MiB chip → upper 2 MiB of flash; 2 MiB chip → mirror of `0x0–0x1FFFFF`. Top edge unverified — see caveat below |
| `0x003F8000`–`0x003FFFFF`   | 32 KiB | Upper SRAM bank (TCM-like, undocumented for this chip variant) — see below |
| `0x00400000`–`0x0043FFFF`   | 256 KiB | RAM Block 1 (main firmware RAM)    |
| `0x00440000`–`0x007FFFFF`   | —      | RAM Block 1 mirrors                 |
| `0x00800000`–`0x008FFFFF`   | 1 MiB  | Peripheral bus                      |
| `0x00900000`–`0x0093FFFF`   | 256 KiB | RAM Block 2 (IRAM / coprocessor)   |
| `0x01000000`                | —      | MDM / AGC (modem + baseband AGC)    |
| `0x01050000`                | —      | RC (RF transceiver control)         |
| `0x01060000`                | —      | MPB (MAC PHY Bypass)                |
| `0x10910000`                | —      | INTC (NX coprocessor interrupt controller) |
| `0x10E00000`                | —      | LA (logic analyzer)                 |
| `0xC0000000`–`0xC001006C`   | —      | NXMAC (MAC core + PL + PTA)         |

---

## Flash (XIP)

Physical capacity: 2 MiB or 4 MiB depending on the installed chip variant (one built-in SPI NOR flash).

### CRC interleaving

The flash controller operates in a CRC-aware XIP mode for the application
region. Every 32 bytes of logical data is stored as 34 physical bytes: 32 data
bytes followed by a 2-byte CRC16-CCITT (polynomial `0x8005`, initial value
`0xFFFF`, big-endian). The controller strips the CRC bytes on read.

Mapping from logical byte offset `n` to physical flash byte offset:

```
physical = (n / 32) * 34 + (n % 32)
```

Consequence: logical `0x00010000` (linker `ORIGIN`) maps to physical flash byte
`0x00011000` (programmed by the flasher at `--startaddr 0x11000`).

For the standard partition layout and flasher segment names see `docs/partitions.md`.

For the vendor bootloader exception vector tables and hook dispatch see `docs/bootloader.md`.

### Upper flash window (`0x00200000`–`0x003FFFFF`)

Behavior depends on the installed flash chip variant:

- **2 MiB chip** — the XIP controller aliases the entire 2 MiB flash into both
  halves of the 4 MiB window. Reads from `0x00200000`–`0x003FFFFF` return the
  same raw physical bytes as `0x00000000`–`0x001FFFFF` (without CRC decoding).
- **4 MiB chip** — `0x00200000`–`0x003FFFFF` maps to the upper 2 MiB of the
  physical flash (physical bytes `0x200000`–`0x3FFFFF`), not a mirror. These
  bytes are accessible raw via the OPERATE_SW path and as CRC-decoded XIP data
  via the XIP path, the same as the lower window.

Address bits above the flash capacity are insignificant (standard SPI flash
addressing), so `0x400000` is used as a base for all dump segments — it aliases
physical `0x000000` on both chip variants. On a 4 MiB chip consecutive sectors
at `0x400000, 0x401000, …, 0x7FF000` correctly wrap through the full 22-bit
physical space, covering both halves.

`uartreader --segment full2m` dumps 2 MiB from physical `0x0`.
`uartreader --segment full4m` dumps 4 MiB from physical `0x0` (for 4 MiB chips).

**Caveat:** the top of this window, `0x003F8000`–`0x003FFFFF`, is not flash at
all — see [Upper SRAM Bank](#upper-sram-bank-0x003f80000x003fffff) below.
Immediately below that bank, `0x003F7F00` hangs the CPU bus outright rather
than returning flash-mirror data. The exact extent of that hang and whether
ordinary flash-mirror reads resume further down were not characterized —
treat reads anywhere near the top of this window (roughly the last 64 KiB)
as unverified until probed.

---

## RAM Block 1

Base: `0x00400000`.  Size: 256 KiB (`0x00400000`–`0x0043FFFF`).

The block is mirrored at `0x00440000`–`0x007FFFFF` (7 additional 256 KiB
aliases). Firmware uses the canonical `0x004xxxxx` range; mirrors are not used.

### Hook table

`0x00400000`–`0x0040001F` (32 bytes). Written by the bootloader before handing
control to the application. The application re-writes these slots at init time
(`boot_init.c`) to redirect exceptions to its own handlers. See `docs/bootloader.md`
for how the vendor bootloader dispatches through these slots.

| Address        | Symbol                   | Exception            |
|----------------|--------------------------|----------------------|
| `0x00400000`   | `hook_IRQ`               | IRQ                  |
| `0x00400004`   | `hook_FIQ`               | FIQ                  |
| `0x00400008`   | `hook_SWI`               | Software interrupt   |
| `0x0040000C`   | `hook_undefined`         | Undefined instruction |
| `0x00400010`   | `hook_pabort`            | Prefetch abort       |
| `0x00400014`   | `hook_dabort`            | Data abort           |
| `0x00400018`   | `hook_reserved`          | Reserved             |
| `0x0040001C`   | —                        | Unused / padding     |

### Firmware RAM layout (flash build)

| Address range               | Section              | Notes                                 |
|-----------------------------|----------------------|---------------------------------------|
| `0x00400020`–end of `.bss`  | `.data` + `.bss`     | Linker `RAM` region; `.data` copied from flash at startup |
| end of `.bss` → bottom of stack | Heap             | `end` symbol; grows upward            |
| `0x43E030`–`0x43FFFF`       | Mode stacks          | See table below; grows downward       |

### Mode stack layout

Stacks sit at the top of RAM Block 1. Each mode's stack is filled with a
canary pattern at boot; overflow detection reads the first word.

| Address range               | Mode    | Size   | Canary       |
|-----------------------------|---------|--------|--------------|
| `0x43FC10`–`0x43FFFF`       | SYS     | 0x3F0  | `0xEEEEEEEE` |
| `0x43F420`–`0x43FC0F`       | FIQ     | 0x7F0  | `0xDDDDDDDD` |
| `0x43E430`–`0x43F41F`       | IRQ     | 0xFF0  | `0xCCCCCCCC` |
| `0x43E040`–`0x43E42F`       | SVC     | 0x3F0  | `0xBBBBBBBB` |
| `0x43E030`–`0x43E03F`       | UNUSED  | 0x010  | `0xAAAAAAAA` |

ABT and UND modes share the UNUSED stack (they are not expected to trigger in
normal operation).

---

## RAM Block 2 (IRAM)

Base: `0x00900000`.  Size: 256 KiB (`0x00900000`–`0x0093FFFF`).

Idle state is `0xAA`-filled. Used by the IRAM build variant (`iram.lds`):
vectors and code load and run from this block, allowing fast firmware iteration
without writing flash. Under normal flash-boot operation this block may be
reserved for RF/MAC or a hardware coprocessor; its use by that subsystem is not
fully characterised.

IRAM firmware is loaded via `tools/bkloader iram` over UART and entered directly
at `0x00900000`.

**Instruction cache covers flash, not IRAM.** The chip has an instruction cache
that covers the flash XIP address space. Code running from flash executes at rated
speed (cache hits, zero extra wait states). IRAM addresses are not cached; every
instruction fetch from IRAM is a cache miss and pays full memory latency.

Measured effect: effective instruction throughput from IRAM is ~57 % of rated CPU
speed (stable 4/7 ratio across clock divider settings: ~68.6 MHz throughput at
120 MHz rated, ~91.4 MHz at 160 MHz rated).

Practical consequence: `busy_wait_*` and any other loop calibrated to a fixed
cycles-per-iteration count delivers **~1.75× the requested delay** when executing
from IRAM. Use `hardware_timer` for accurate delays in IRAM code.

---

## Upper SRAM Bank (`0x003F8000`–`0x003FFFFF`)

Base: `0x003F8000`. Size: 32 KiB (`0x003F8000`–`0x003FFFFF`), directly below
RAM Block 1 (`0x00400000`).

Not present in this project's own linker scripts (a `TCM` region was added
purely for future use — see below), and not part of the vendor SDK's memory
map for this specific chip: `beken_freertos_sdk/application.mk` links the
`bk7251` target (BK7221U, this chip) against `bk7231.ld` / `bk7231_bsp.ld`,
which define no `tcm`/`itcm` region at all. A same-named region *is* defined
in the vendor SDK's linker scripts for other chips in the family —
`bk7231n.lds` (`tcm` 56–60 KiB + `itcm` 4–8 KiB, ending exactly at
`0x00400000`), `bk7238_bsp.lds` (`itcm` 16 KiB, `0x3F0000`–`0x3F4000`),
`bk7252n_bsp.lds` / `bk7253_bsp.lds` (`itcm` 32 KiB, `0x3F0000`–`0x3F8000`) —
none of those boundaries match what was measured here, so this is not simply
an unused copy of another chip's TCM layout; treat it as a distinct,
undocumented-for-this-variant hardware block.

Confirmed by on-chip probing (`run-on-chip` skill):

- **Readable and writable.** Full address-as-data sweep (every word written
  with its own address, then read back): 8192/8192 words matched, no
  aliasing.
- **Executable.** A small ARM routine copied to and run from `0x3F8000`,
  `0x3FC000`, and `0x3FFFF0` all executed correctly.
- **Idle-state fill:** `0xAAAAAAAA`, same convention as RAM Block 2 (IRAM).
- **Faster than IRAM for instruction fetch.** A calibrated busy loop ran at
  ~57 % of the wall-clock time it took in IRAM (i.e. ~1.75× the instruction
  throughput) across most of the bank — consistent with single-cycle,
  non-wait-stated access (TCM-like), unlike IRAM's shared-bus wait states
  (see RAM Block 2 above). This does not carry over to explicit data
  access — see [Data Access Speed](#data-access-speed-tcm-vs-ram-block-1-vs-iram)
  below, where RAM Block 1 outperforms TCM for `LDR`/`STR`.
- **Edge slowdown.** The last ~48 bytes before `0x00400000` (between
  `0x3FFFC0` and `0x3FFFF0`) drop to ~71 % of IRAM time — still faster than
  IRAM, but a clear regression right at the RAM Block 1 boundary, likely a
  pipeline/prefetch interaction with the adjacent bank rather than a
  property of the SRAM itself.

**Sharp lower boundary — genuine bus hang, not backed by data.** `0x3F7F00`
(256 B below `0x3F8000`) hangs the CPU bus outright — not a Data Abort;
confirmed by a `wdt_set` watchdog firing and the vendor bootloader's normal
POR boot sequence appearing after reset, rather than a caught exception. The
exact lower edge of the hang was not narrowed further below `0x3F7F00`, nor
was it established whether ordinary flash-mirror reads (see Flash upper
window, above) resume further down. Do not read below `0x3F8000` without
first re-probing.

Exposed in `src/linker/flash.lds`, `iram.lds`, and `bootloader.lds` as a
`TCM` memory region (declared only — no section currently placed there).

---

## Data Access Speed: TCM vs RAM Block 1 vs IRAM

Confirmed by on-chip probing (`run-on-chip` skill),
measuring explicit `LDR`/`STR` word access with code always executing from
IRAM (so instruction-fetch cost is constant across the comparison) — two
2048-word buffers per region, 30 repetitions, timed with `hardware_time`:

| Region                                   | write (ns/word) | read (ns/word) | copy (ns/word-pair) |
|-------------------------------------------|-----------------|-----------------|----------------------|
| RAM Block 1 (`0x00400000`)                | 108.4           | 83.4            | 91.7                 |
| Upper SRAM bank / TCM (`0x003F8000`)      | 108.4           | 91.7 (+10 %)    | 100.0 (+9 %)         |
| RAM Block 2 / IRAM (`0x00900000`)         | 133.4 (+23 %)   | 108.4 (+30 %)   | 141.7 (+55 %)        |

**RAM Block 1 is the fastest region for explicit data access — the opposite**
ranking from instruction-fetch speed (see Upper SRAM Bank above, where TCM
beats IRAM for code). TCM ties RAM on writes but is measurably slower on
reads and copies; IRAM is slowest throughout, and disproportionately worse
on copy, consistent with bus contention — the benchmark's own code fetches
instructions from the same IRAM bank it is simultaneously loading from and
storing into.

A warm-up pass (touching every buffer word once, untimed, before the timed
loop) ruled out `.bss` pre-zeroing as an artifact — result unchanged. Note:
this measures data throughput only, with code fixed in IRAM for all three
regions; it does not isolate the cost of running code from RAM Block 1 or
TCM (no such build variant exists).

---

## Instruction Fetch Speed: TCM vs IRAM vs RAM Block 1

Confirmed by on-chip probing (`run-on-chip` skill). A
12-byte ARM routine with an exactly-known instruction count (`subs`+`bne`,
2 instructions/iteration, 1,000,000 iterations = 2,000,000 instructions) was
compiled once, then the identical bytes were `memcpy`'d to a TCM address and
a RAM Block 1 address and invoked via function pointer — same code, only the
execution address changes, isolating pure fetch latency:

| Region                          | time (2 M instr.) | ns/instruction | throughput    | % of rated 120 MHz |
|-----------------------------------|--------------------|-----------------|----------------|----------------------|
| Upper SRAM bank / TCM (`0x003F8000`) | 33,334 µs       | 16.67           | 60.0 M-insn/s  | 50 %                 |
| RAM Block 2 / IRAM (`0x00900000`)    | 58,334 µs       | 29.17           | 34.3 M-insn/s  | 29 %                 |
| RAM Block 1 (`0x00400000`)           | —                | —               | **traps: `bk_trap_udef`** | n/a       |

**TCM is 1.75× faster than IRAM for code** — reproducible to within 1 µs
across repeated runs. This independently confirms, via a rigorous
known-instruction-count method, the earlier "~57 % of IRAM wall-clock time"
busy-loop finding in the Upper SRAM Bank section above (1/1.75 ≈ 57 %).

**RAM Block 1 cannot execute code at all.** Copying the identical 12 bytes
there and calling them traps immediately with an Undefined Instruction
exception (`bk_trap_udef`), reproducible across two runs. Likely
explanation: RAM Block 1 is wired only to the CPU's data-side bus, not the
instruction-fetch bus, unlike TCM and IRAM which both serve as genuine
fetch-capable memory. This matches the `RAM (rw!x)` attribute already
declared for this region in `src/linker/flash.lds`, `iram.lds`, and
`bootloader.lds` — that attribute was a linker-policy convention (no
executable section should be placed there), not previously a cited,
tested hardware fact; it is now empirically confirmed, with a known,
specific failure mode (an immediate trap, not silent corruption or a bus
hang).

Caveat: the loop is `subs`+`bne` (conditional branch every iteration), which
costs more cycles than branch-free code due to pipeline refill — these
absolute MHz figures are specific to this loop and not comparable to the
busy-loop-based ~57 % IRAM figure above (different instruction mix). Only
the cross-region **ratio** (1.75×) is robust across instruction mixes.

---

## Peripheral Bus (`0x00800000`–`0x008FFFFF`)

All peripheral registers are 32-bit word-aligned. Byte and halfword accesses
to peripheral registers are not supported unless noted.

### Peripheral base addresses

| Base address   | Peripheral     | MMIO header               | Driver                          | Notes                                              |
|----------------|----------------|---------------------------|---------------------------------|----------------------------------------------------|
| `0x00800000`   | SCTRL          | `soc/sctrl.h`             | `hardware_sctrl`                | System control: clocks, power, PLL, resets         |
| `0x00800074`   | eFuse          | `soc/efuse.h`             | `hardware_efuse`                | Embedded in SCTRL register space (`SCTRL + 0x1D×4`) |
| `0x00801000`   | SDIO           | `soc/sdio.h`              | —                               | SDIO slave DMA engine (master/slave, 1–4 bit, ≤50 MHz) |
| `0x00802000`   | ICU / INTC     | `soc/icu.h`               | `hardware_icu`, `hardware_intc` | Interrupt Controller Unit; clock gating and interrupt routing share this block |
| `0x00802100`   | UART1          | `soc/uart.h`              | `hardware_uart`                 | —                                                  |
| `0x00802200`   | UART2          | `soc/uart.h`              | `hardware_uart`                 | —                                                  |
| `0x00802300`   | I2C1           | —                         | —                               | —                                                  |
| `0x00802400`   | IRDA           | —                         | —                               | IrDA NEC decoder and UART-mode receiver            |
| `0x00802480`   | TRNG           | `soc/random.h`            | `hardware_random`               | True Random Number Generator                       |
| `0x00802500`   | I2S / PCM      | —                         | —                               | I2S/PCM audio interface; 3 TX FIFOs + 1 RX FIFO   |
| `0x00802600`   | I2C2           | —                         | —                               | —                                                  |
| `0x00802700`   | SPI            | `soc/spi.h`               | —                               | SPI master/slave, 8/16-bit, up to 30 MHz           |
| `0x00802800`   | GPIO Bank 0    | `soc/gpio.h`              | `hardware_gpio`                 | GPIO 0–31 (`+0x00` per pin)                        |
| `0x008028C0`   | GPIO Bank 1    | `soc/gpio.h`              | `hardware_gpio`                 | GPIO 32+ (`GPIO_BASE + 48×4`)                      |
| `0x00802900`   | WDT            | `soc/wdt.h`               | `hardware_wdt`                  | Watchdog timer                                     |
| `0x00802A00`   | Timer Bank 0   | `soc/timer.h`             | `hardware_timer`                | 3 timers, 26 MHz source                            |
| `0x00802A40`   | Timer Bank 1   | `soc/timer.h`             | `hardware_timer`                | 3 timers, 32 kHz source (`PWM_NEW_BASE + 0x10×4`)  |
| `0x00802A80`   | PWM            | `soc/pwm.h`               | `hardware_pwm`                  | 6-channel PWM/timer (`PWM_NEW_BASE + 0x20×4`); modes: PWM output, timer IRQ, capture |
| `0x00802B00`   | Audio          | —                         | —                               | Audio codec: ADC, DAC, DTMF generator              |
| `0x00802C00`   | SAR ADC        | —                         | —                               | Successive-approximation ADC                       |
| `0x00802D00`   | SD card host   | —                         | —                               | SD/MMC host controller (BK7221U register layout)   |
| `0x00803000`   | Flash ctrl     | `soc/flash.h`             | `hardware_flash`                | SPI flash controller (XIP config, erase, program)  |
| `0x00804000`   | USB            | —                         | —                               | USB 2.0 FS OTG; byte-addressed MUSB-style register map |
| `0x00805000`   | FFT            | —                         | —                               | Hardware FFT accelerator                           |
| `0x00806000`   | Security — AES | `soc/security.h`          | `hardware_security`             | AES sub-block (`SECURITY_BASE + 0×4`)              |
| `0x00806100`   | Security — SHA | `soc/security.h`          | `hardware_security`             | SHA sub-block (`SECURITY_BASE + 0x40×4`)           |
| `0x00806200`   | Security — RSA | `soc/security.h`          | —                               | RSA sub-block (`SECURITY_BASE + 0x80×4`); not implemented — SDK has no `.c` exercising it, register layout unverified |
| `0x00808000`   | SPI DMA        | —                         | —                               | SPI slave DMA engine; RX/TX FIFOs with timeout interrupt |
| `0x00809000`   | GDMA           | `soc/gdma.h`              | `hardware_gdma`                 | General-purpose DMA, 6 channels                    |
| `0x0080A000`   | JPEG encoder   | `soc/jpeg.h`              | `hardware_jpeg`                 | Hardware JPEG encoder                              |
| `0x0080D000`   | QSPI           | —                         | —                               | Quad-SPI controller                                |

### FFT internal memory (`0x00805800`–`0x00805FFF`)

Confirmed by on-chip probing (`run-on-chip` skill):

- **Clock-gated.** Reads as a stable `0xAAAAAAAA` and rejects writes while
  `hw_icu->peri_clk_pwd.fft` (bit 19) is set (the reset default). Clearing
  that bit exposes it as genuine read/write RAM — full address-as-data sweep
  (each word written with its own address, then read back): 512/512 words
  matched, no aliasing.
- **Idle-state fill:** `0xAAAAAAAA` while gated, same convention as RAM Block 2
  (IRAM) and the Upper SRAM Bank. This is the chip-wide idle pattern, not a
  hardware test/ID register — it only reads that way because the block's
  clock is off by default.
- **Size:** 2048 bytes (512 words), `0x00805800`–`0x00805FFF`, ending exactly
  at the next peripheral (Security, `0x00806000`).
- **Does not require `FFT_CONF.ENABLE`** (bit 3 of `0x00805000`, vendor SDK
  `beken378/driver/fft/fft.h`) — the clock gate alone unlocks it; `FFT_CONF`
  can stay at its reset value.

**Discrepancy with the vendor SDK:** `beken378/driver/fft/fft.h` defines
`FFT_MEMORY_BASE_ADDR` as `FFT_BASE + 0x200` (`0x00805200`), not `+0x800`. That
address (`0x00805200`–`0x008057FF`) remains non-writable even with both the
clock gate cleared and `FFT_CONF.ENABLE` set — it behaves like unmapped space
(reads `0`, writes don't stick), not RAM. Not characterised further.

`0x00805020`–`0x008051FF` (between the FFT control registers at
`0x00805000`–`0x0080501C` and `FFT_MEMORY_BASE_ADDR`) reads `0` and rejects
writes regardless of clock/enable state — genuinely empty, not gated RAM.

---

## Upper Address Space

### RC — `0x01050000`

RF transceiver control block (`hw_rc`, `soc/rc.h`). Spans
`0x01050000`–`0x010501A8` (107 words). `TRX_REG28` is accessed at the
anomalous address `0x08628078`, outside all mapped regions.

### MPB — `0x01060000`

MAC PHY Bypass (`hw_mpb`, `soc/mpb.h`). Two sub-blocks: main at
`0x01060000` (`hw_mpb`, `r0`–`r11`) and TX-vector extra at `0x01060200`
(`hw_mpb_extra`, `r128`–`r143`).

### MDM / AGC — `0x01000000`

Modem and AGC registers share a single base address; sub-groups occupy
distinct offset windows:

| Sub-group   | Offset range          | Count | Notes                                                    |
|-------------|-----------------------|-------|----------------------------------------------------------|
| MDM STAT    | `+0x0000`–`+0x0068`   | 27    | Read-only status; `+0x0000` = version register           |
| MDM CFG     | `+0x0800`–`+0x0894`   | 550   | Config registers                                         |
| AGC         | `+0x2000`–`+0x20AC`   | 2092  | Only part of the 2092-register space is described in SDK |
| PHY AGC µcode | `+0xA000`           | —     | SDK comment warns writes to the mirror alias `0x01C0A000` may not be correct on this SoC |

`REG_MDM_CFG_SIZE = 152` and `REG_AGC_SIZE = 172` are SDK-internal constants
that do not reflect total byte sizes (those are COUNT × 4).

### INTC (NX) — `0x10910000`

Interrupt controller for the NX coprocessor subsystem. Size: 68 bytes (17
registers). Distinct from the ARM ICU at `0x00802000`.

### LA — `0x10E00000`

Logic analyzer block. 16 registers. Offset constant `REG_LA_OFFSET = 0x00800000`
suggests a second window or mirror at `0x10E00000 + 0x800000 = 0x11600000`.

### NXMAC — `0xC0000000`

NX 802.11 MAC hardware core. Three sub-blocks:

| Sub-block  | Base           | Size    | Registers | Notes                                    |
|------------|----------------|---------|-----------|------------------------------------------|
| MAC Core   | `0xC0000000`   | 1376 B  | 344       | Core MAC engine                          |
| MAC PL     | `0xC0008000`   | 1404 B  | —         | Physical layer interface                 |
| MAC PTA    | `0xC0010000`   | 36 B    | —         | Packet traffic arbitration; `NXMAC_CONFIG` at `0xC0010004` |

Monotonic free-running counters at `0xC000011C`–`0xC0000124` are used as
microsecond timebase by `hardware_time`.
