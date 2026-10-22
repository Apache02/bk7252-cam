# BK7252 emulator

`tools/emulator/` runs this project's firmware on a PC, in a terminal, without the board.
It emulates the CPU with Unicorn and the peripherals with small Python modules. The chip is
described by a profile file; images are placed by command-line arguments.

## Setup

```sh
python3 -m venv tools/emulator/.venv
tools/emulator/.venv/bin/pip install -r tools/emulator/requirements.txt
```

`pyserial` is needed only to bridge a UART to a real host serial device.

## Run

```sh
tools/emulator/run BK7252 \
    --load bootloader:build-release/out/bootloader_crc.bin \
    --load app:build/app_crc.bin
```

The launcher uses the venv automatically. On start it prints the host end of each UART:

```
UART1: /dev/pts/6
UART2: /dev/pts/8
```

Open the UART2 line like a board: `tio -b 115200 /dev/pts/8`. Stop with Ctrl-C.

Options:

| Option | Meaning |
| ------ | ------- |
| `PROFILE` | Profile name, upper case: `profiles/PROFILE.toml`. |
| `--load TARGET:FILE` | Copy FILE byte for byte to TARGET. Repeatable, at least one is required. |
| `-d`, `--debug MODULE[,MODULE...]` | Print debug logs (`gdma`, `flash`, `wdt`, `wifi`, or `wifi.rc` for one WiFi block). Several names with commas, or repeat the option: `-d gdma,wifi`. |
| `--uart1 SPEC`, `--uart2 SPEC` | Host end of the UART: `pty` (default), `stdio`, or a serial device path. |

Log lines go to stderr. When stderr is a terminal, warnings are yellow and errors red; `NO_COLOR=1`
turns the colour off and `FORCE_COLOR=1` turns it on for a pipe or a file.

`TARGET` is an alias from the profile (case-insensitive) or a number. Aliases of `BK7252`:

| Alias | Where |
| ----- | ----- |
| `RAM`, `IRAM` | RAM block 1 and RAM block 2 |
| `bootloader`, `app` | Raw flash offsets `0x0` and `0x11000` |

Flash aliases point at **raw physical** flash, so a `*_crc.bin` file is loaded as it is. A
flash load must start on a 34-byte block boundary and be a multiple of 34 bytes. Numeric
addresses are accepted only inside RAM; loading into a module's range or outside memory is an
error reported before the emulator starts.

## What is emulated

| Block | Notes |
| ----- | ----- |
| CPU | Unicorn, ARM946 model (closest to ARM968E-S). SWI, undefined instruction, aborts, IRQ and FIQ are entered the way the core does. |
| RAM | `RAM1`, `RAM2`. Code cannot run from `RAM1`: a fetch there logs `bus error: fetch from non-executable memory` and raises a Prefetch Abort, as on the chip (`executable = false` in the profile). Data access works. |
| Flash | 2 MiB raw store and the CRC-decoded XIP window. A block with a bad CRC logs an `ALERT` line and raises a Data Abort while `conf.crc_en` is set. Controller commands: read ID, read and write status registers, write enable/disable, 32-byte read, 32-byte page program (clears bits only), sector, 32 KB and 64 KB block erase, chip erase. The write rules follow `docs/hardware/flash.md`: a command needs `WP#` released and a set write-enable latch (consumed by it); block-protection bits in the status register stop programs and erases (a rejected command logs a warning). Changes show up in the XIP window at once. So `tools/bkloader flash` and a reboot into the written app work. The contents live in memory only and are lost when the emulator exits. `--debug flash` prints every erase, program, status-register write and write-enable change, decoded (reads are not logged). |
| SCTRL | `chip_id` and `device_id` (read-only, set in the profile); every other register reads back what was written, starting from the profile's `reset_words` (`warm_reset_words` after a chip reset). No analog or clock behaviour. A profile hack (`read_adjust`) makes the MCLK divider (`control[7:4]`) read one higher than written, so busy-wait loops, which count iterations from it, run shorter in emulated time (the emulator is several times slower than the chip). A firmware read-modify-write of that register stores the biased value back, so the divider drifts up to its maximum; `cpu_speed` and `mclk` show the biased frequency. |
| GDMA | Six channels, memory to memory, with the byte-accumulator model of `docs/hardware/dma.md` (widths, +4 address steps, zero-padded tail) and its transfer times. The copy lands when the modelled time has passed; `enable` clears, the finish and half-finish flags and the interrupt follow. Channels with a peripheral request line stall and repeat mode runs once, both with a warning. A bus error aborts the channel with an error line. `--debug gdma` prints every configuration, start, completion and acknowledge decoded. |
| WiFi blocks | The register banks of `src/platform/soc/include/rwnx`, one module each in `modules/wifi/`: MAC (core and PL blocks together), MAC PTA, modem config and status, AGC, AGC microcode RAM (`0x0100A000`, 2 KiB), RC, NX interrupt controller, NX DMA, LA. Most are plain memory (what is written reads back, bytes and halfwords included) with no behaviour, so status bits that hardware would clear stay as written. The MAC has behaviour, modelled after the RivieraWaves interface the firmware drives: the state register (`state_cntrl`; leaving a state for IDLE raises the IDLE interrupt, which the firmware waits for before it starts an AP), the general and TX/RX interrupt registers and their six ICU FIQ lines (17-22), the ten ABS timers, the TSF, the TBTT interrupt, the key storage RAM and the time-on-air calculator. The monotonic counters (`monotonic_counter_1` at 40 MHz, `monotonic_counter_2_lo/_hi` in microseconds) run while the MAC is powered (the SCTRL power register holds the wake key) and read 0 otherwise. The MAC PL soft-reset bit (`mac_cntrl_2`, +0x50) clears itself at the next tick. Frames are not sent or received: TX queue kicks and RX buffers do nothing yet. `--debug wifi` logs every read and write of every WiFi block; `--debug wifi.mac_core` picks one block. |
| GPIO | 40 pins in two banks. A pin drives its pad while its output driver is on; otherwise the pad follows the level forced from the host side, then the pull resistor, then reads 0. Interrupts: level low/high, rising/falling edge. Peripheral functions of a pin are not modelled. |
| TRNG | Not random, so runs are repeatable: `data` returns a read counter (0, 1, 2, ...) and `ctrl.enable` is 1 from the start and after a chip reset. While disabled, `data` is a plain latch, as in `docs/hardware/random.md`. |
| ICU | Interrupt enable, status, global IRQ/FIQ enables. |
| UART1, UART2 | 128-byte RX FIFO, instant TX, level and latched interrupts. |
| Timers | Two banks (26 MHz and 32 kHz), three periodic timers each. |
| WDT | Period in ms and the `0x5A`/`0xA5` reload keys. Counts only while the ICU clock-power-down bit for it is 0 (it is 1 at reset). Expiry resets the chip. `-d wdt` logs every reload ("kick"): the period, the time since the previous kick as a percent of its period (close to 100% means the chip nearly reset) and how far the reset moved. |

A chip reset (watchdog expiry, so also the shell `reboot` command) fills every memory region that has a
`reset_fill` in the profile with that byte (both RAM blocks of BK7252 use `0xAA`; a region without
it keeps its contents), clears the general registers, returns every module to its reset state, keeps the flash contents and
restarts the CPU at address 0. The UART endpoints stay connected.

Anything else in the peripheral window (`0x00800000`-`0x008FFFFF`) is logged once per address as
`unclaimed peripheral` and reads as 0. An access to memory nothing answers logs
`bus error: unmapped access` and raises a Data or Prefetch Abort.

Time is a virtual clock driven by executed instructions. When the CPU waits for an interrupt
the clock jumps to the next timer event.

## Profile

`profiles/<NAME>.toml` holds the instruction rate, the peripheral window, `[[memory]]` regions (each with an optional `reset_fill` byte and an `executable` flag, true by default),
`[alias]` entries (a number is a bus address; a table `{module, offset}` points into a module)
and `[[module]]` instances (`type`, `name`, `[module.bases]` per address range, `[module.params]`).
A module that raises interrupts names its controller and source bit with `intc` and `irq`; the
controller must be listed first. A module that watches a register bit of another module takes a
param ending in `_signal`, such as `clock_off_signal = "ICU.arm_wdt_pwd"`; the owner declares the
bit under its own `signals` param. Every module also gets `clock` and `reset_chip` constructor
arguments.

## Adding a module

1. Write `tools/emulator/modules/<name>.py`: subclass `core.module.Module`, return
   `(range_name, size)` pairs from `ranges()`, implement `read`/`write`, and `tick`/`next_event_ns`
   if it depends on time. Take collaborators (`clock`, `irq`, `endpoint`, ...) as constructor
   arguments.
2. Register the class in `modules/__init__.py`.
3. Add a `[[module]]` entry to the profile.

The core never imports a module and a module never imports another module.

## Tests

```sh
tools/emulator/.venv/bin/python -m unittest discover -s tools/emulator/tests -t tools/emulator
```

Booting a real image is a manual check.

## Limits

One chip. Flash is not saved between runs. The WiFi MAC sends and receives no frames, the other
WiFi blocks are plain memory, and there are no JPEG or camera sensor modules, so DMA from the JPEG request line
stalls. The GPIO module has a Python interface (`set_input`, `pad_level`, `watch`) for
tests and scripts, but the command line does not expose it; firmware that needs them runs into the log lines above.
