# BK7252 Camera Firmware

Platform exploration firmware for the BK7252-based A9 camera module — built without
the vendor SDK. Drivers talk directly to hardware registers reverse-engineered from
the vendor binary and disassembly. Work in progress: the platform layer (boot, clocks,
drivers, shell) is functional; higher-level camera functionality is not yet implemented.

**Chip:** BK7252 / ARM968E-S (up to 180 MHz) &nbsp;|&nbsp;
**Flash:** 2–4 MiB SPI NOR &nbsp;|&nbsp;
**RAM:** 256 KiB + 256 KiB IRAM &nbsp;|&nbsp;
**Toolchain:** `arm-none-eabi-gcc` (C11 / C++20)

---

## Requirements

```sh
# Ubuntu / Debian
sudo apt install cmake build-essential gcc-arm-none-eabi \
                 libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
sudo apt install python3 python3-hid python3-serial python3-tqdm
```

A 3.3 V USB–UART adapter connected to UART2 (the shell port), 115200 baud.

---

## Build

```sh
mkdir -p build
cd build && cmake -DBOARD=A9_B_V1_3 .. && make <target>
```

Each application target produces a flashable `<target>_crc.bin` and, when
`bk_firmware_iram()` is also called in its `CMakeLists.txt`, an IRAM variant
`<target>--iram` (see [IRAM](#iram)).

Smoke-build all targets without a board attached:

```sh
./smoke_build.sh
```

---

## What's here

### Applications (`src/applications/`)

Flashable firmware images. Targets marked **+iram** also produce an IRAM variant.

| Target | +iram | What it does |
|--------|:-----:|-------------|
| `hello` | ✓ | Prints to UART. Bare-metal, no RTOS. |
| `blink` | | Blinks a GPIO. Bare-metal, no RTOS. |
| `shell` | ✓ | Bare-metal interactive shell. GPIO, timer, SCTRL commands. |
| `freertos_example` | | Minimal FreeRTOS demo: two tasks blinking and printing. |
| `freertos_shell` | ✓ | FreeRTOS + lwIP + interactive shell. Most complete app — good starting point. Includes commands for GPIO, eFuse, AES/SHA. |
| `ram_loader` | | Minimal shell (`loadi` / `go` / `speed`) for loading IRAM images. Flash once, then iterate without touching flash. |
| `bootloader` | ✓ | Custom replacement for the vendor bootloader — see [Bootloader](#bootloader). |

### Driver tests (`src/tests/`)

IRAM-only images — no flash write needed. Require `ram_loader` or `bootloader`
running on the chip. All test targets have the suffix `--iram`.

| Target | What it tests |
|--------|--------------|
| `test_security` | AES-128 encrypt/decrypt and SHA-256 against known vectors. |
| `test_gdma` | DMA channel transfers with timing. |
| `test_random` | TRNG output sanity. |

---

## Flash & monitor

The right tool depends on which bootloader is installed:

```sh
# --- vendor bootloader (factory default) ---
tools/flasher/uartprogram --segment app build/app_crc.bin   # flash
tools/flasher/uartreader  --segment app backups/app_crc.bin    # backup

# --- custom bootloader (after installing it) ---
tools/bkloader flash build/app_crc.bin   # flash app partition
tools/bkloader backup                    # backup app to backups/

# --- serial monitor (either case) ---
tio -b 115200 /dev/ttyUSB0              # adjust port as needed
```

> The build always produces a CRC-wrapped image (`app_crc.bin`). The BK7252 flash
> controller requires interleaved CRC — never flash the raw `.bin`.

Transfer speed can be increased with `--speed 921600` (both tools support it).

---

## IRAM

RAM Block 2 (`0x00900000`, 256 KiB) can run firmware directly. Loading to IRAM
does not touch flash, making it the preferred workflow for driver development and
hardware experiments.

### How it works

`tools/bkloader iram` automates the full sequence:

1. Sends Ctrl+C, then `reboot` to enter the bootloader shell.
2. Optionally switches to a faster baud rate (`--speed`).
3. Sends `loadi` — chip opens a receive window, host streams the binary.
4. Verifies CRC32 on both sides.
5. Sends `go 0x00900000` — chip jumps to the loaded image.

The chip must be running a shell with `loadi`, `go`, and `speed` commands —
either `ram_loader` or `bootloader` flashed.

### Usage

```sh
# build an IRAM image
(cd build && make freertos_shell--iram)

# load and run; keep port open, capture output for 10 s
tools/bkloader iram build/app_iram.bin --capture 10

# faster transfer
tools/bkloader iram build/app_iram.bin --speed 921600 --capture 30

# shell is already at prompt — skip reboot
tools/bkloader iram build/app_iram.bin --no-reboot --capture 10

# load but don't jump yet
tools/bkloader iram build/app_iram.bin --no-jump
```

`--until STRING` stops capture as soon as the output contains that string —
useful for automated pass/fail checks.

---

## Bootloader

`src/applications/bootloader` replaces the vendor bootloader in the `bootloader`
flash partition (`0x000000–0x00FFFF`). Instead of the vendor's binary UART
protocol it provides an interactive shell — similar to U-Boot or GRUB.

**What it provides:**
- `loadi` / `go` / `speed` — IRAM load workflow (used by `bkloader`).
- `flash_read` / `flash_write` / `buffer` — raw flash access (used by `bkloader`).
- `reboot` — reboot with autoboot countdown, interruptible for shell access.

**Compatibility:** tested with the original camera firmware — the camera boots and
operates correctly with this bootloader in place.

**After installing:** `tools/flasher/uartprogram` and `uartreader` stop working
(they speak the vendor protocol). Use `tools/bkloader` for all subsequent flashing.

### Installing

The vendor bootloader cannot erase/write its own partition while executing from it.
The only installation path is via `bootloader_installer` — an IRAM app that runs
entirely from RAM and writes the bootloader partition from there.

```sh
# 1. Flash ram_loader to the app partition (vendor bootloader protocol)
tools/flasher/uartprogram --segment app build/ram_loader_crc.bin

# 2. Load and run bootloader_installer from RAM
(cd build && make bootloader_installer--iram)
tools/bkloader iram build/bootloader_installer--iram.bin --capture 15
```

`bootloader_installer` writes the embedded `bootloader_crc.bin` to the bootloader
partition and reboots into the new bootloader.

---

## Implemented drivers

`src/platform/drivers/` — one CMake library per peripheral, link only what you use.

UART · GPIO · SPI Flash · GDMA · TRNG · AES-128 / SHA-256 · Timers (×6) ·
System control / PLL · ICU · INTC · eFuse · Watchdog · Microsecond timebase

Register layouts only (no driver code yet): I2C · SAR ADC · FFT · JPEG · RSA

### Known gaps

- **`timer_read()`** — exported but not working (marked in source).
- **AES / SHA** — hardware has one context; concurrent use produces incorrect output. Use the one-shot helpers only.

---

## Extending

**New shell command** — drop a `.cpp` into `src/shell/shell_commands_*/commands/`
or `src/applications/freertos_shell/commands/`. Auto-discovered by `file(GLOB)`.

**New application** — `src/applications/<name>/CMakeLists.txt` with
`add_executable` + `bk_firmware(...)`. Auto-discovered.

**New driver test** — `src/tests/<name>/CMakeLists.txt` with
`add_executable` + `bk_firmware_iram(...)`. Auto-discovered.

---

## Docs

- [`docs/memory_map.md`](docs/memory_map.md) — address space, peripheral bases, RAM layout
- [`docs/partitions.md`](docs/partitions.md) — flash partition table, CRC interleaving
- [`docs/bootloader.md`](docs/bootloader.md) — vendor bootloader internals, PLL init sequence
- [`docs/known_issues.md`](docs/known_issues.md) — deferred bugs and architectural follow-ups
- [`docs/hardware/`](docs/hardware/) — per-peripheral hardware references
