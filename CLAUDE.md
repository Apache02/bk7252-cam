# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Bare-metal / FreeRTOS firmware for a BK7252-based A9 camera (ARM968E-S, thumb-interwork). Cross-compiled with `arm-none-eabi-gcc` (C11 / C++20, `-O2`, `--specs=nano.specs`, `-fno-exceptions`).

## Build

CMake out-of-tree build. The board must be selected (defaults to `A9_B_V1_3`):

```sh
mkdir -p build
(cd build && cmake -DBOARD=A9_B_V1_3 .. && make freertos_shell)
```

Other application targets (see `src/applications/`): `hello`, `blink`, `shell`, `freertos_example`, `freertos_shell`, `random_test`, `ram_loader`.

Each firmware target produced by `bk_firmware()` (see `cmake/firmware.cmake`) post-processes the ELF into `<target>.bin` and a CRC-wrapped `<target>_crc.bin`, copying the latter to `build/app_crc.bin`. Targets wrapped with `bk_firmware_iram()` additionally produce an IRAM-linked variant (`<target>--iram`) using `src/linker/iram.lds` and write `build/app_iram.bin`.

Smoke-build all main targets in a throwaway directory:

```sh
./test_build.sh
```

There is no test framework — `test_build.sh` only verifies clean compilation.

## Flash / backup / monitor

```sh
./flash.sh              # tools/flasher/uartprogram --segment app build/app_crc.bin
./backup.sh             # dumps app segment from /dev/ttyUSB0 into backups/
tio -b 115200 /dev/ttyUSB0
```

Flasher tools (`tools/flasher/uartprogram`, `uartreader`) are Python scripts using `tools/flasher/bkutils/`. Other Python helpers: `tools/crc` (wraps a flat `.bin` with the BK CRC layout — invoked automatically by `bk_firmware()`), `tools/uncrc`, `tools/iram_loader`, `tools/console_dump/dump.py`.

## Architecture

Layered CMake tree under `src/` — each subdir is its own CMake library and gets composed by the application:

- `src/platform/` — chip/runtime layer. Each subdir is one library:
  - `boot/` (`platform_boot`) — ARM vector table + reset handler + `boot_init.c`. All firmware targets link this via `bk_firmware()`.
  - `nosys/` — minimal libc syscall stubs.
  - `freertos/` (`platform_freertos`) — FreeRTOS kernel wrapper + port for this CPU.
  - `port_newlib/`, `port_lwip/`, `port_tlsf/` — adapters between the upstream library (fetched in `dependencies.cmake`: FreeRTOS-Kernel V11.1.0, lwIP STABLE-2_2_0, tlsf) and this firmware (locks, allocators, OS shim).
  - `port_beken/` — vendor-supplied Beken IP code (libs + headers used for radio/MAC/etc.).
  - `drivers/` — one library per on-chip peripheral (`hardware_gpio`, `hardware_uart`, `hardware_flash`, `hardware_wdt`, …). Apps link only the drivers they use.
  - `stdio/` — `printf`/`getchar` backends bound to a chosen UART (e.g. `platform_stdio_uart2`). Note: `printf` is built without float (see README).

- `src/shell/` — interactive UART shell, split so an app can pick which command groups it ships:
  - `shell_main` — `Shell`, `Parser`, `History`, `Table` — the engine, no commands.
  - `shell_commands_common` / `_freertos` / `_beken` / `_iram` — independent command-group libraries. An app links the groups it wants.

- `src/applications/` — final firmware images. Each `add_executable(...)` here calls `bk_firmware(target)` (and optionally `bk_firmware_iram(target)`), wiring `platform_boot`, the chosen `platform_stdio_uart*`, FreeRTOS heap variant, lwip port, shell engine + command groups, and the specific drivers it needs. `freertos_shell` is the canonical reference for how to compose a real app.

- `src/boards/` — board pin/feature headers (`A9_B_V1_3.h`, `default.h`). The `BOARD` CMake var (or `$ENV{BOARD}`) selects the active board and adds `BOARD_<NAME>` as a compile definition exposed via the `board_config` INTERFACE library.

- `src/linker/` — `flash.lds` (XIP from flash @ `0x00010000`, RAM @ `0x00400020`) and `iram.lds` (load-and-run from RAM block 2 @ `0x00900000`). The first 0x20 bytes of RAM are reserved for the bootloader's ARM exception hook table — see `docs/memory_map.txt` for the full chip memory map.

- `src/utils/` — generic helpers (`crc`, `busy_wait`, `panic.c`).

External dependencies are fetched into `libs/` by `dependencies.cmake` via `FetchContent` (FreeRTOS-Kernel, lwIP, tlsf). Don't edit them in place.

## Conventions worth knowing

- All targets are warnings-as-errors (`-Wall -Werror` in `cmake/toolchain.cmake`); a stray warning will fail the build.
- Adding a new shell command: drop a `.cpp` into the relevant `shell_commands_*/commands/` directory — they're picked up by `file(GLOB)`. New apps under `src/applications/freertos_shell/commands/` likewise glob-included by that target's `CMakeLists.txt`.
- CRC wrapping (`tools/crc`) is required to produce a flashable image — never flash the raw `*.bin`, always `*_crc.bin` / `app_crc.bin`.
- The IRAM build (`*--iram` target, `app_iram.bin`) is for loading and running directly out of RAM block 2 via `tools/iram_loader` — useful for fast iteration without writing flash.
- All source content (identifiers, comments, strings, docs) must be English-only.
- Do not run the build (`cmake`, `make`, `./test_build.sh`) — the user runs builds themselves.
