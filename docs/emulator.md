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
| `--uart1 SPEC`, `--uart2 SPEC` | Host end of the UART: `pty` (default), `stdio`, or a serial device path. |

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
| RAM | `RAM1`, `RAM2`. |
| Flash | 2 MiB raw store and the CRC-decoded XIP window. A block with a bad CRC logs an `ALERT` line and raises a Data Abort while `conf.crc_en` is set. Controller commands: read ID, read status, write enable/disable, 32-byte read. Erase and program are logged and not performed. |
| ICU | Interrupt enable, status, global IRQ/FIQ enables. |
| UART1, UART2 | 128-byte RX FIFO, instant TX, level and latched interrupts. |
| Timers | Two banks (26 MHz and 32 kHz), three periodic timers each. |

Anything else in the peripheral window (`0x00800000`-`0x008FFFFF`) is logged once per address as
`unclaimed peripheral` and reads as 0. An access to memory nothing answers logs
`bus error: unmapped access` and raises a Data or Prefetch Abort.

Time is a virtual clock driven by executed instructions. When the CPU waits for an interrupt
the clock jumps to the next timer event.

## Profile

`profiles/<NAME>.toml` holds the instruction rate, the peripheral window, `[[memory]]` regions,
`[alias]` entries (a number is a bus address; a table `{module, offset}` points into a module)
and `[[module]]` instances (`type`, `name`, `[module.bases]` per address range, `[module.params]`).
A module that raises interrupts names its controller and source bit with `intc` and `irq`; the
controller must be listed first.

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

One chip. Flash erase and program are not emulated. No WiFi, JPEG, camera sensor, GPIO, DMA,
system control or watchdog modules yet; firmware that needs them runs into the log lines above.
