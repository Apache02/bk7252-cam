# Development

Building the firmware, running code from RAM, and the layout of the source tree. To just flash a
release, see the [README](../README.md).

**Chip:** BK7252 / ARM968E-S (up to 180 MHz) &nbsp;|&nbsp;
**Flash:** 2–4 MiB SPI NOR &nbsp;|&nbsp;
**RAM:** 256 KiB + 256 KiB IRAM &nbsp;|&nbsp;
**Toolchain:** `arm-none-eabi-gcc` (C11 / C++20)

The firmware is built without the vendor SDK. Drivers talk directly to hardware registers
reverse-engineered from the vendor binary and disassembly.

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

The `camera` target also builds its web frontend, so it needs Node.js 20.19+ and
`npm ci` run once in `src/applications/camera/frontend`.

---

## Releases

Release images are built in Docker, so the compiler and Node versions are the same on
every machine. Needs Docker and network access.

```sh
git tag v0.1.0
./release.sh
```

The script builds the committed tree (`HEAD`, uncommitted changes are left out) and
writes the images into `dist/<version>/`. The board comes from `BOARD` (default
`A9_B_V1_3`, `BOARD=<name> ./release.sh` builds another one into the same directory). `uart2` is
the console port the images are linked with. Only the camera image depends on the board; the
loaders are the same for every board, so their names have no board.

| File | What it is |
|------|------------|
| `<board>-camera-<version>-uart2_crc.bin` | The camera firmware, ready to flash. |
| `ram_loader-<version>-uart2_crc.bin` | A minimal shell for loading RAM images. Flashed with the vendor tool, only to run the installer. |
| `bootloader_installer-<version>-uart2_iram.bin` | A RAM image that writes the custom bootloader (built into it) to flash, see [Custom bootloader](custom-bootloader.md). |

The version is `git describe --tags`; it is printed at boot and shown on the camera System page.
Between tags it looks like `v0.1.0-3-g1a2b3c4`.

---

## Flash & monitor

The right tool depends on which bootloader is installed:

```sh
# --- vendor bootloader (factory default) ---
tools/flasher/uartprogram --segment app build/app_crc.bin   # flash
tools/flasher/uartreader  --segment app -o backups/app_crc.bin    # backup
# backup with a port and a timestamped name:
mkdir -p backups
tools/flasher/uartreader -p /dev/ttyUSB0 --segment app \
    -o backups/app_$(date +%Y-%m-%d_%H%M%S)_crc.bin

# --- custom bootloader (after installing it) ---
tools/bkloader flash build/app_crc.bin   # flash app partition
tools/bkloader backup                    # backup app to backups/

# --- serial monitor (either case) ---
tio -b 115200 /dev/ttyUSB0              # adjust port as needed
```

> The build always produces a CRC-wrapped image (`app_crc.bin`). The BK7252 flash
> controller requires interleaved CRC — never flash the raw `.bin`.

Transfer speed can be increased with `--speed 921600` (both tools support it).

To install the [custom bootloader](custom-bootloader.md#installing) from your own build instead of a
release, build the two images. Every build copies its image to `build/app_crc.bin`
(IRAM: `build/app_iram.bin`):

```sh
(cd build && make ram_loader)
tools/flasher/uartprogram --segment app build/app_crc.bin

(cd build && make bootloader_installer--iram)
tools/bkloader iram build/app_iram.bin --capture 15
```

---

## What's here

### Applications (`src/applications/`)

Flashable firmware images. Targets marked **+iram** also produce an IRAM variant.

| Target | +iram | What it does |
|--------|:-----:|-------------|
| `camera` | | The camera firmware: WiFi (access point or station), web page, MJPEG stream, telnet console, settings in flash. |
| `hello` | ✓ | Prints to UART. Bare-metal, no RTOS. |
| `blink` | | Blinks a GPIO. Bare-metal, no RTOS. |
| `shell` | ✓ | Bare-metal interactive shell. GPIO, timer, SCTRL commands. |
| `freertos_example` | | Minimal FreeRTOS demo: two tasks blinking and printing. |
| `freertos_shell` | ✓ | FreeRTOS + lwIP + interactive shell. Good starting point for a new application. |
| `freertos_wifi` | | FreeRTOS + lwIP + WiFi shell commands. |
| `ram_loader` | | Minimal shell for loading IRAM images (`loadi` / `go` / `speed`) and reading flash and memory. It cannot write flash. Flash once, then iterate without touching flash. |
| `bootloader` | ✓ | Custom replacement for the vendor bootloader — see [Custom bootloader](custom-bootloader.md). |
| `bootloader_installer` | ✓ | IRAM image that writes a bootloader to flash. |

### Driver tests (`src/tests/`)

IRAM-only images — no flash write needed. Require `ram_loader` or `bootloader`
running on the chip. All test targets have the prefix `test_` and the suffix `--iram`
(for example `test_gdma--iram`).

| Directory | What it tests |
|-----------|--------------|
| `security` | AES-128 encrypt/decrypt and SHA-256 against known vectors. |
| `gdma` | DMA channel transfers with timing. |
| `random` | TRNG output sanity. |
| `i2c1_scan` | I2C1 bus scan. |
| `uart_tx` | UART transmit. |
| `flash_kv` | The flash key-value store. Destroys the settings sector. |
| `probe` | Scratch template for hardware experiments. |

### Drivers (`src/platform/drivers/`)

One CMake library per peripheral, link only what you use: eFuse, flash, GDMA, GPIO, I2C, ICU,
INTC, JPEG, PWM, TRNG, system control / PLL, AES / SHA, timers, UART, watchdog, microsecond
timebase.

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

`--until PATTERN` stops capture as soon as a line contains the pattern —
useful for automated pass/fail checks. `*` matches any text. The flag can be
repeated; the first match wins. For example, `--until "==END==" --until "Bootloader*"`
also stops when the chip resets back into the bootloader (watchdog, panic, assert).

---

## Extending

**New shell command** — drop a `.cpp` into `src/shell/shell_commands_*/commands/`
or `src/applications/freertos_shell/commands/`. Auto-discovered by `file(GLOB)`.

**New application** — `src/applications/<name>/CMakeLists.txt` with
`add_executable` + `bk_firmware(...)`. Auto-discovered.

**New driver test** — `src/tests/<name>/CMakeLists.txt` with
`add_executable` + `bk_firmware_iram(...)`. Auto-discovered.

---

## Other docs

- [`memory_map.md`](memory_map.md) — address space, peripheral bases, RAM layout
- [`partitions.md`](partitions.md) — flash partition table, CRC interleaving
- [`bootloader.md`](bootloader.md) — vendor bootloader internals, PLL init sequence
- [`custom-bootloader.md`](custom-bootloader.md) — this project's bootloader and how to install it
- [`known_issues.md`](known_issues.md) — deferred bugs and architectural follow-ups
- [`hardware/`](hardware/) — per-peripheral hardware references
