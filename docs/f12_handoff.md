# F12: CPU loads from IRAM go wrong while the GDMA reads a peripheral

Evidence behind F12 in `docs/known_issues.md`. Everything here was measured on the board unless it is marked
*guess*.

## Result

While the GDMA loads the shared bus, a CPU data load from RAM block 2 (IRAM, `0x00900000`) can return the word
the CPU is fetching as instructions at that moment. Literal pools sit in IRAM next to the code, so constants,
pointers and branch targets are loaded wrong now and then. Memory itself is never changed.

It needs both at once: instructions fetched from IRAM and data read from IRAM. Either one alone is correct under
the same DMA load.

GDMA reads of a peripheral register are the strongest trigger: one such channel is enough. Memory-to-memory
transfers need more load: one or two channels are clean, six crash the CPU.

Every F12 symptom follows from that: wrong values, wild jumps, `bk_trap_udef`, `bk_trap_swi`, false
`bk_trap_pabt`, assertions. The "freezes" are endless loops: a misread timer base gives a time that never
reaches the deadline, and the loop keeps feeding the watchdog. A flashed build reads code and literals from
flash and is stable.

## Setup

- BK7252 (ARM968E-S, ARMv5TE), board `HB6_V1_0D211115`, HI704 sensor on I2C2, console on UART1.
- Own bootloader; images loaded with `tools/bkloader iram --speed 921600 --port /dev/ttyUSB0`. Code and rodata
  in IRAM; `.data`, `.bss` and stacks in RAM block 1 (`0x00400000`); bootloader clock (no `sctrl_set_cpu_freq_hz`
  unless noted). IRQ enabled, FIQ masked.
- Memory facts from `docs/memory_map.md`: RAM block 1 cannot run code; IRAM is on the shared bus with wait
  states; the 32 KiB bank at `0x003F8000` runs code and is faster than IRAM.

## Probe

`git stash list` on `feat/camera-ring-capture`, entry "probe: F12 hclk_div2 ..." (older entries, each lacking
what came after it: "probe: F12 matrix mode ...", "probe: F12 compare mode ...", "probe: F12 multi-channel
...", "probe: F12 root cause ..."). `src/tests/probe/`
holds `probe.cpp` and `probe_arm.S`. The stashed `CMakeLists.txt` adds an include directory for `probe_cfg.h`
that pointed at a temporary directory; point it at any directory holding a `probe_cfg.h` with the macros
below (an empty file selects the defaults).

The probe runs a scan routine (ARM or Thumb asm) over a 4096-word table, `table[i] = i * 0x9E3779B1 ^
0x5A5AA5A5`, in chunks of 50 passes (`RUN_MS / 56` chunks; the loop is bounded by the chunk count, because
the timer base is itself a misread literal), with a GDMA channel in repeat mode writing a 16 x 1468 byte
ring. The scan recomputes each expected value from the two constants, which it loads from its literal pool on
every iteration. A mismatch is reported as `@addr got= expected= reread= now=`. The odd multiplier makes any
32-bit value traceable back to its index.

Undefined, SWI, prefetch and data abort hooks are installed right before the DMA starts (the interrupt driver
rewrites the whole hook table when it first starts) and print `TRAP <kind>` with r0-r12, the exception LR,
SPSR, the interrupted mode's SP and LR, the code around the fault and the top of the stack. The hooks run from
IRAM too, so with the DMA still running a dump can come out empty.

| Macro | Values |
|---|---|
| `CODE_LOC` | 0 scan routine in IRAM, 2 copied to `0x003F8000` |
| `TABLE_LOC` | 0 RAM block 1, 1 IRAM (`0x00928000`), 2 `0x003FA000` |
| `RING_LOC` | 0 RAM block 1, 1 IRAM (`0x00920000`), 2 `0x003F9000` |
| `DMA_SRC` | 0 JPEG FIFO (camera on), 1 memory, 2 no DMA (camera on), 3 `sctrl` `chip_id` at `0x00800000` (camera off) |
| `DMA_INTERVAL` | 0..15, source and destination interval for sources 1 and 3 |
| `DMA_PRIO_FIXED`, `DMA_PRIO` | fixed arbitration on, channel priority |
| `EXTRA_CH`, `EXTRA_SRC`, `EXTRA_IN_DTCM` | 0..5 more channels, source 3 `chip_id` or 1 memory, each into its own 2 KiB window in IRAM (`0x00926000 + k * 0x800`) or in RAM block 1; interval 15 |
| `REBOOT_WITH_DMA` | 1: watchdog reset right after the channels start, to see whether the bootloader comes up |
| `CMP_MODE`, `CMP_PASSES` | 1: compare 48 KiB of flash at `0x0` (the bootloader image), 2: 48 KiB of IRAM at `0x00928000`, with a copy in RAM block 1, `CMP_PASSES` times (default 200). The compare routine `cmp_begin` has no literal pool and runs from `0x003F8000` with interrupts off, so nothing is fetched from IRAM while the DMA runs |
| `MATRIX`, `MX_WORK`, `MX_CH`, `MX_SRC`, `MX_PASSES`, `MX_WAKES` | `MATRIX=1`: `MX_CH` channels (1..6), source `MX_SRC` 0 flash at `0x0`, 1 IRAM (`0x00930000 + k * 0x800`, a pattern), 2 `chip_id`, each into a 2 KiB window in IRAM (`0x00926000 + k * 0x800`), interval 15, round-robin. `MX_WORK` 1: compare 48 KiB of flash `MX_PASSES` times (default 800); 2: WFI with IRQ masked, woken by the GDMA finish line `MX_WAKES` times (default 20000), clearing `int_status` after each wake. The routines (`mx_cmp_begin`, `mx_wfi_begin`) run from `0x003F8000` and start and stop the channels themselves; afterwards every destination word is checked against its source (`dma_bad`, `dma_zero` = never written) |
| `HCLK_DIV2` | 1: set `sctrl` `ctrl.hclk_div2_en` at start, by one store from `hclk_set` copied to `0x003FF000`, with IRQ masked and a spin after it; the `SCTRL` line prints the register before and after |
| `HCLK_CLEAR` | 1: force `ctrl.hclk_div2_en` off (baseline without the divider; the current bootloader sets it) |
| `RUN_BY_TIME` | 1: bound the run by `RUN_MS` of wall time instead of a chunk count (only safe when the code runs from flash) |
| `SCAN_THUMB`, `RUN_MS`, `CPU_FREQ` | Thumb scan (default 1), run length, CPU clock in Hz (0 keeps the bootloader clock) |

Fastest repro: `RING_LOC=1 DMA_SRC=3 DMA_INTERVAL=15`. With `DMA_PRIO_FIXED=1` the misreads repeat exactly
from run to run (one build: 2291816 per 9 s run, no crash); a later build of the same test crashed, because
the repeating misreads then hit a different load.

Build `make test_probe--iram`, run `tools/bkloader iram ... --capture 20 --until "==END==" --until
"Bootloader*" build/app_iram.bin`. One run takes about 25 s. A run that ends without a result and without the
bootloader banner is an endless loop that feeds the watchdog; the board then needs a power cycle.

## Key captures

Literal load returns the fetch word. Thumb scan in IRAM, JPEG DMA into IRAM:

```
@0x0041310c got=0xf8dfdc58 expected=0xcf9072a8 reread=0xf8dfdc58
```

`got` is the correct `table[0xc0d]`. `expected` equals `0xc0d * 0x2f043701 ^ 0x5A5AA5A5`, and `0x2f043701` is
the word at `0x009009f8` (`adds r7, #1; cmp r7, #4`), 12 bytes ahead of the `ldr r6, [pc, #60]` at
`0x009009ec` that should have loaded `0x9E3779B1` from `0x00900a2c`. The `chip_id` runs with fixed
arbitration show the same `0x2f043701` in every miss.

The earlier "corrupted words with byte 3 equal to byte 2" (old table `i * 0x01010101 ^ 0x5A5AA5A5`) are the
same thing: `got` matches the table formula, `expected` was computed from a misread constant.

Wild jumps land on real function entries: `command_platform_stdio_speed` printing "Invalid arguments" followed
by `bk_trap_swi`; the platform `do_pabort` reporting `bk_trap_pabt` while the probe's own prefetch hook never
ran; a call into the table address `0x00928000` (two runs, same place) from `main`, where both the table
address and the function address are literals. *Guess*: each is a pointer loaded from the wrong word.

Older trap reports showed the same `R0 0x000003e8 R1 0xfffffeff R2 0x00802000` everywhere. Those are the
registers of the panic printer, not of the faulting code.

## Matrix

Thumb scan, 9 s per run unless noted.

| Code | Table | DMA source → ring | Runs | Result |
|---|---|---|---|---|
| IRAM | block 1 | JPEG → block 1 | 29 | 2 runs with 1 misread, 4 crashes, 23 clean |
| IRAM | block 1 | JPEG → IRAM | 8 | about 220 misreads in each of 5 runs, 3 crashes |
| IRAM | block 1 | JPEG → IRAM, fixed priority 1 | 4 | 225-245 misreads each, 0 crashes |
| IRAM | block 1 | memory → IRAM, interval 0 | 4 | CPU starved, no output, watchdog reset |
| IRAM | block 1 | memory → IRAM, interval 15 | 3 | clean |
| IRAM | block 1 | `chip_id` → IRAM, interval 15 | 3 | crash in the first 50 ms each |
| IRAM | block 1 | same, 26 MHz CPU | 2 | crash (watchdog reset) |
| IRAM | block 1 | same, fixed priority 1 or 7 | 5 | 2291816 misreads each, 0 crashes (a later build: crash) |
| IRAM | block 1 | `chip_id` → block 1, interval 15 | 1 | froze, power cycle needed |
| `0x3F8000` | block 1 | JPEG → IRAM | 4 | scan 0 misreads in 2 full runs; 1 crash in IRAM code; 1 load failure |
| `0x3F8000` | block 1 | `chip_id` → IRAM, fixed priority | 1 | crash in IRAM code |
| `0x3F8000` | IRAM | `chip_id` → IRAM, fixed priority | 2 | wild call into the table from `main` (IRAM) |

Several channels, IRAM code, table in block 1, main ring in IRAM, interval 15, fixed priority 1. The loop was
bounded by time in the first runs, by chunk count in the ones below. "First chunk" rows stop after 50 passes
(204800 loads).

| Channels | Runs | Result |
|---|---|---|
| 2 memory (the second into block 1) | 1 | clean, 9 s |
| 6 memory | 1 | UND trap before the first chunk ended |
| `chip_id` + 1 memory | 2 | crash, silent watchdog reset |
| `chip_id` + 1 memory, first chunk | 1 | 33590 misreads (16 %) |
| 2 `chip_id` | 1 | crash, silent watchdog reset |
| 2 `chip_id`, first chunk | 3 | 30694 (15 %) in both runs of one build (time-bounded loop); 808 (0.4 %) in a later build |
| `chip_id` + 5 memory | 1 | warm reset (jump to `0x0`) |
| 1 memory, `REBOOT_WITH_DMA=1`; same with 2 memory channels | 2 | bootloader banner after the watchdog reset |

The misread rate depends on which loads the repeating pattern hits, so it changes with every build; the
numbers show "more load, more misreads", not a stable rate.

The 6-memory-channel UND trap: Thumb state at `0x00905f5a`, the second half of the ARM `b __udivsi3` right
after the `bx pc` of the `____aeabi_uidiv_from_thumb` veneer at `0x00905f54`. The memory there read back
correct. *Guess*: the `bx pc` was not executed as fetched, i.e. an instruction fetch went wrong.

Compare mode: routine in `0x003F8000`, interrupts off, 200 passes over 48 KiB, `chip_id` channel into IRAM at
interval 15 (round-robin) where noted.

| Reads | DMA | Runs | Misreads | Time per pass |
|---|---|---|---|---|
| IRAM | none | 1 | 0 | 1433 us |
| IRAM | `chip_id` | 3 | 0 | 1450 us (+1.2 %) |
| flash (bootloader at `0x0`) | none | 1 | 0 | 1241 us |
| flash (bootloader at `0x0`) | `chip_id` | 1 | 0 | 1241 us, identical |

So data loads from IRAM are correct under the DMA as long as no instruction is fetched from IRAM; the slowdown
shows IRAM does share the bus with the DMA. Flash reads are not slowed at all. 48 KiB in 1241 us is about
40 MB/s, which is at the limit of the SPI flash itself, so these reads may have been served by a cache.

Matrix mode: everything that runs during the DMA is in `0x003F8000`, interrupts masked. One run per cell.

| Channels | Flash compare, DMA from flash / IRAM / `chip_id` | WFI, DMA from IRAM / `chip_id`: time per wake |
|---|---|---|
| 1 | clean / clean / clean | 72 us / 72 us |
| 2 | clean / clean / clean | 36 us / 36 us |
| 4 | clean / clean / clean | 24 us / 24 us |
| 6 | clean / clean / clean | 17 us (one run) / 17 us |

"Clean" means 0 misreads and 1241 us per flash pass, the same value as without the DMA. Every WFI run woke
20000 times and returned. In every run the DMA wrote correct data (`dma_bad=0`). The same six `chip_id`
channels crash IRAM code within milliseconds.

The GDMA does not read flash. With a flash source each channel wrote about 6 of its 512 words (`dma_zero` 506
per channel) and then stalled. A WFI run with such a channel never woke, since no finish interrupt came; the
watchdog did not reset the chip within the 20 s capture either, and the board needed a power cycle. Why the
watchdog did not fire there is not known.

AHB clock divider. The vendor SDK sets `ctrl.hclk_div2_en` (bit 9) on BK7221U together with the MCU clock
whenever it sets up the PLL, and clears it only for sleep: "BK7221U ahb bus max rate is 90MHZ, so ahb bus need
div 2 from MCU clock. AHB bus is very import to AUDIO and DMA" (`sys_ctrl.c`). The bootloader leaves `ctrl` at
`0x00021032` (bit 9 clear), so the AHB runs at the MCU clock, about 120 MHz. That was the old bootloader:
the current one sets the bit (`ctrl` reads `0x00021232` at boot), and `sctrl_set_cpu_freq_hz` sets it for
clocks above 90 MHz. The table below was measured before that change.

| Test (scan loop in IRAM) | Without the bit | With the bit (switched from `0x003FF000`) |
|---|---|---|
| `chip_id` → IRAM, interval 15 | 5020160 misreads, or crashes | 6 of 6 clean |
| JPEG → block 1 | 23 of 29 clean | 6 of 8 clean, 1 watchdog reset, 1 cut off after a wrong time read |
| JPEG → IRAM | 0 of 8 clean | 2 of 6 clean |

So the bit removes the misreads under a steady single channel but not under the bursty JPEG traffic. *Guess*:
the halved bus clock moves the steady pattern off the vulnerable moment, while JPEG bursts still hit it.

Setting the bit directly from IRAM code, even with a 5 ms wait after it, crashed in 5 of 7 runs before the DMA
started (`bk_trap_udef` before the first line of output, or a read-back of `0x00000000`). Switching it from
`0x003FF000` with interrupts masked worked in 17 of 17 runs.

Earlier session (`git stash` entry "probe: F12 minimal repro ..."): memory-to-memory DMA into block 1 was clean
for 6 s; a register-integrity loop survived 80k GDMA interrupts; the IRAM code region compared equal to a
snapshot after failures.

Mode stack high-water marks in these runs: SVC 816 of 1008 bytes, IRQ 196 of 4080, FIQ and SYS unused, the
shared ABT/UND stack 8 of 16 (the bootloader's `push {r0, r1}`) after a trap.

### Retest with the divider on (2026-11-11)

Board A9_B_V1_3 (UART2 console, no camera), current bootloader (divider already set). Scan loop in IRAM,
table in block 1, `chip_id` channel into IRAM, interval 15. The probe needs `DMA_PRIO >= 1` whenever
`DMA_PRIO_FIXED` is on: a fixed-priority channel with priority 0 is never granted the bus, so the DMA never
moves and the run looks falsely clean (three such runs were discarded). `HCLK_CLEAR=1` forces the bit off from
`0x003FF000` for the baseline.

| CPU clock | Divider | Priority mode | Runs | Result |
|---|---|---|---|---|
| 160 MHz | off | fixed 1 | 1 | watchdog reset in the first chunk |
| 160 MHz | off | round-robin | 1 | watchdog reset |
| 120 MHz | off | round-robin | 1 | watchdog reset |
| 160 MHz | on | fixed 1 | 3 | wrong: 233 or 288 errors, loop counter and timer base corrupted |
| 160 MHz | on | round-robin | 1 | wrong, 288 errors |
| 180 MHz (DCO) | on | fixed 1 | 2 | wrong, 233 / 292 errors |
| 120 MHz | on | fixed 1 | 1 | wrong, 288 errors |
| 120 MHz | on | round-robin | 2 | wrong, 288 errors |
| 80 MHz | off (below 90 MHz) | fixed 1 | 1 | watchdog reset |

No IRAM run lasted long enough for the planned 300 s. The divider makes the failure milder (a finished run with
misreads instead of a reset in most cases) but does not remove it. The 288-error runs repeat the same
numbers, so the misreads are deterministic for a given build. The JPEG trigger was not run from IRAM in this
retest (the camera was connected only later).

Same probe built as a flash image (`bk_firmware(test_probe)` added next to `bk_firmware_iram`, `RUN_BY_TIME=1`),
310 s per run, camera connected:

| Trigger | CPU clock | Divider | Result |
|---|---|---|---|
| `chip_id` → IRAM, fixed priority 1 | 160 MHz | on | 8350 chunks, 0 errors |
| `chip_id` → IRAM, fixed priority 1 | 180 MHz | on | 9403 chunks, 0 errors |
| `chip_id` → IRAM, fixed priority 1 | 160 MHz | off | 8350 chunks, 0 errors |
| JPEG stream → IRAM ring | 160 MHz | on | 8350 chunks, 0 errors, ring filled |
| JPEG stream → IRAM ring | 180 MHz | on | 9473 chunks, 0 errors, ring filled |

Full application: `camera` flashed, 180 MHz + divider, WiFi STA, `/stream` pulled for 320 s: 2528 frames, no
crash, no reset; the board logged 101 dropped frames.

## Open questions

1. Can instruction fetches from IRAM themselves return wrong words? Every decoded case was a data load, but the
   6-memory-channel trap above points that way.
2. Which peripheral reads trigger it: any APB register, or only some? Only `chip_id` and the JPEG FIFO were
   tried. How much memory-to-memory load is needed: two channels are clean, six are not; three to five were
   not tried alone.
3. Is the flash data path affected when the reads miss the cache? The compare runs read only 48 KiB, which may
   have stayed cached. A larger flash region (for example with `crc_en` cleared, so any region can be read)
   would answer it. Instruction fetch from flash under the DMA was not tested from this probe; flashed builds
   streaming JPEG are stable.
4. Why did the watchdog not reset the chip while the CPU slept in WFI with a stalled flash-source channel?
5. Does `hclk_div2_en` set at boot, together with the PLL as the vendor does, behave differently from setting it
   later at run time? Only the run-time switch was tested.

Answered: a data load from IRAM does not fail when no instruction is fetched from IRAM at the same time (see
the compare-mode table).
