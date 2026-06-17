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

Other application targets (see `src/applications/`): `hello`, `blink`, `shell`, `freertos_example`, `freertos_shell`, `ram_loader`. Standalone driver/peripheral tests live under `src/tests/` (currently `test_gdma`, `test_random`, `test_security`) — they are separate IRAM firmware images, not a unit-test harness.

`src/CMakeLists.txt` auto-globs both `applications/` and `tests/` — dropping a new subdirectory with its own `CMakeLists.txt` into either is enough to register it as a target.

Each firmware target produced by `bk_firmware()` (see `cmake/firmware.cmake`) post-processes the ELF into `<target>.bin` and a CRC-wrapped `<target>_crc.bin`, copying the latter to `build/app_crc.bin`. Targets wrapped with `bk_firmware_iram()` additionally produce an IRAM-linked variant (`<target>--iram`) using `src/linker/iram.lds` and write `build/app_iram.bin`.

Smoke-build the main app targets in a throwaway directory:

```sh
./smoke_build.sh
```

There is no unit-test framework — `smoke_build.sh` only verifies clean compilation of `blink hello shell freertos_example freertos_shell`.

## Flash / backup / monitor

```sh
./flash.sh              # tools/flasher/uartprogram --segment app build/app_crc.bin
./backup.sh             # dumps app segment from /dev/ttyUSB0 into backups/
tio -b 115200 /dev/ttyUSB0
```

Flasher tools (`tools/flasher/uartprogram`, `uartreader`) are Python scripts using `tools/flasher/bkutils/`. Other Python helpers: `tools/crc` (wraps a flat `.bin` with the BK CRC layout — invoked automatically by `bk_firmware()`), `tools/uncrc`, `tools/bkloader`, `tools/console_dump/dump.py`, `tools/symbol_table.py`.

## Architecture

Layered CMake tree under `src/` — each subdir is its own CMake library and gets composed by the application:

- `src/platform/` — chip/runtime layer. Each subdir is one library:
  - `boot/` (`platform_boot`) — ARM vector table + reset handler + `boot_init.c` + low-level ARM CPU primitives (`cpu.S`, exported as `platform/cpu.h`: IRQ/FIQ enable/disable, `WFI`, alignment-fault control). All firmware targets link this via `bk_firmware()`.
  - `nosys/` — minimal libc syscall stubs.
  - `panic/` — panic handler.
  - `freertos/` (`platform_freertos`) — FreeRTOS kernel wrapper + port for this CPU.
  - `port_newlib/`, `port_lwip/`, `port_tlsf/` — adapters between the upstream library (fetched in `dependencies.cmake`: FreeRTOS-Kernel V11.1.0, lwIP STABLE-2_2_0, tlsf) and this firmware (locks, allocators, OS shim).
  - `drivers/` — one library per on-chip peripheral. Apps link only the drivers they use. Current set: `hardware_efuse`, `hardware_flash`, `hardware_fft`, `hardware_gdma`, `hardware_gpio`, `hardware_i2c`, `hardware_icu`, `hardware_intc`, `hardware_jpeg`, `hardware_mpb`, `hardware_random`, `hardware_rc`, `hardware_saradc`, `hardware_sctrl`, `hardware_security`, `hardware_time`, `hardware_timer`, `hardware_uart`, `hardware_wdt`, plus `hardware_common` (shared register-base/utility headers used by the drivers; lives in `drivers/common/`; provides `register_defs.h` with the `hw_write_fields()` macro for atomic multi-field bitfield writes — linked transitively by every driver).
  - `stdio/` — `printf`/`getchar` backends bound to a chosen UART (e.g. `platform_stdio_uart2`). Note: `printf` is built with `--specs=nano.specs` and without float support — `%f`/`%g`/`%e` and 64-bit specifiers `%llu`/`%lld` do not work.

- `src/shell/` — interactive UART shell, split so an app can pick which command groups it ships:
  - `shell_main` — `Shell`, `Parser`, `History`, `Table` — the engine, no commands.
  - `shell_commands_common` / `_freertos` / `_beken` / `_iram` — independent command-group libraries. An app links the groups it wants.

- `src/applications/` — final firmware images. Each `add_executable(...)` here calls `bk_firmware(target)` (and optionally `bk_firmware_iram(target)`), wiring `platform_boot`, the chosen `platform_stdio_uart*`, FreeRTOS heap variant, lwip port, shell engine + command groups, and the specific drivers it needs. `freertos_shell` is the canonical reference for how to compose a real app.

- `src/tests/` — standalone driver smoke tests built as IRAM firmware images (one `add_executable` per subdir, wrapped with `bk_firmware_iram`). Same auto-discovery as `src/applications/`. Loaded via `tools/bkloader iram` without flashing.

- `src/boards/` — board pin/feature headers. `BOARD` CMake var (or `$ENV{BOARD}`, default `A9_B_V1_3`) selects the active board and adds `BOARD_<NAME>` as a compile definition via the `board_config` INTERFACE library; `include/board.h` is the umbrella header consumers include.

- `src/linker/` — three linker scripts: `flash.lds` (XIP from flash @ `0x00010000`, RAM @ `0x00400020`), `iram.lds` (load-and-run from RAM block 2 @ `0x00900000`), and `bootloader.lds` (bootloader image from flash @ `0x00000000`). The first 0x20 bytes of RAM are reserved for the bootloader's ARM exception hook table — see `docs/memory_map.md` for the full chip memory map.

- `src/utils/` — generic helpers (`crc`, `busy_wait`).

- `docs/` — reference documents: `memory_map.md` (chip memory map — address space, flash CRC format, RAM layout, peripheral bases), `known_issues.md` (catalogued firmware bugs / arch issues, prefixed F/A/C/Arch), `hardware/` (per-block hardware references: DMA, Security AES/SHA).

External dependencies are fetched into `libs/` by `dependencies.cmake` via `FetchContent` (FreeRTOS-Kernel, lwIP, tlsf). Don't edit them in place.

## Conventions worth knowing

- All targets are warnings-as-errors (`-Wall -Werror` in `cmake/toolchain.cmake`); a stray warning will fail the build.
- Adding a new shell command: drop a `.cpp` into the relevant `shell_commands_*/commands/` directory — they're picked up by `file(GLOB)`. Per-app commands under `src/applications/freertos_shell/commands/` are likewise glob-included by that target's `CMakeLists.txt`.
- Adding a new application or test: create `src/applications/<name>/CMakeLists.txt` (or `src/tests/<name>/CMakeLists.txt`) with an `add_executable(...)` + `bk_firmware(...)` / `bk_firmware_iram(...)` — `src/CMakeLists.txt` will auto-discover it.
- CRC wrapping (`tools/crc`) is required to produce a flashable image — never flash the raw `*.bin`, always `*_crc.bin` / `app_crc.bin`.
- The IRAM build (`*--iram` target, `app_iram.bin`) is for loading and running directly out of RAM block 2 via `tools/bkloader iram` — useful for fast iteration without writing flash. `tools/bkloader iram` requires a shell with `loadi`, `go`, and `speed` commands running on the chip; `src/applications/ram_loader` is the dedicated app for this purpose.
- All source content (identifiers, comments, strings, docs) must be English-only.
- Do not run the build (`cmake`, `make`, `./smoke_build.sh`) — the user runs builds themselves. Exception: `make test_probe--iram` is allowed when using the `run-on-chip` skill for `src/tests/probe/` experiments.
- `src/tests/probe/` is the scratch template for the `run-on-chip` skill — never commit anything in that directory unless explicitly told to (e.g. when updating the template itself).
