# Known issues and follow-ups

This file tracks issues found during the May 2026 platform review
that were *not* fixed inline in commit `a7cebc0` (`fix: drivers bugfixes and refactor`).
Each entry describes the issue, why it was deferred, and a concrete fix outline.

The entries are grouped by impact. "Functional" can cause silently wrong behaviour;
"API / contract" can blow up in user code; "Cosmetic" is for naming and clarity.
"Architectural" are larger pieces that need a design decision before code changes.

---

## Functional

### F1. AES / SHA engines have a single hardware state — multi-context is unsafe

Files: `hardware_security/aes.c`, `hardware_security/sha.c`.

The driver exposes `*_create_context()` / `_update()` / `_destroy_context()`
as if each context were independent. The on-chip AES and SHA engines have only
one set of key / hash state registers, so the moment a second context is created
(or `process_block` is run on a different context), the previously loaded state
is overwritten. The "first" context now produces incorrect output without any
diagnostic.

Today this is masked because the only callers are the one-shot helpers
(`aes128_encrypt`, `sha256`, …) and the shell test commands — they never
interleave contexts.

Fix options:
- (a) Document the single-context contract and `assert` that only one context
  is live at a time.
- (b) Serialise across contexts: on every `*_update`, reload the key schedule
  (AES) or hash state (SHA) from the in-RAM context, run the block, copy the
  state back. Slower but matches the API as currently advertised.
- (c) Hide multi-context entirely: remove `*_create_context` from the public
  header, expose only the one-shot helpers.

Recommendation: (c) until a real caller needs streaming.


### F8. First ~5 ms of output after `go` is garbled when `bkloader iram` uses `--speed`

File: `tools/bkloader` (host side); the delay itself sits in the tests, e.g.
`src/tests/probe/probe.cpp`.

**`platform_stdio_init()` needs no settle delay.** Right after it returns, the UART
transmits at the correct rate. The delay `busy_wait_ms(20)` that tests put after it is
not needed for the UART.

What is real: when an IRAM image is started with `bkloader iram --speed 921600`, the
first ~5 ms of the program's output arrives as null bytes. After that it is clean.

Measured on the chip (`src/tests/probe/`, October 2026), one labelled line per millisecond
starting right after `platform_stdio_init()`:

| Run | Result |
|---|---|
| no `--speed` | clean from the first byte, `T=00` onward |
| `--speed 921600` | `T=00`..`T=04` garbled (null bytes), `T=05` onward clean |
| `--speed 921600`, one line per 20 ms | only `T=000` garbled, `T=020` onward clean |
| `--speed 921600`, program waits 300 ms **before** `platform_stdio_init()` | clean from `T=00` |

The last row is the one that matters. If the garbling came from the UART settling after
init, it would repeat when the init is moved 300 ms later. It does not. The garbling is
tied to the time since the jump to the program, not to the time since UART init.

The loader talks at 921600 before `go`. The program then runs its UART at 115200, and the
host switches back to 115200 only after the jump. Output sent in between is read at the
wrong rate and comes out as null bytes. This matches the observation, and it is the
likeliest cause. **It is not proven**: `bkloader` waits 0.5 s after `go` before it
switches the port back, not 5 ms, and this is not explained. Running from flash instead
of through `bkloader` has not been tried.

Two earlier descriptions of this entry were wrong:

- The failure was recorded as a hard fault with a register dump and bootloader
  takeover. The chip neither faults nor resets. The dump previously attributed to F8 was
  most likely the deliberate `wdt_reboot()` at the end of a probe.
- The observation that "10 ms produced no output" was a host-side artefact of the old
  `tio` workflow: the loader closed the serial port, discarding the OS receive buffer,
  before `tio` opened it. With `--capture` on `bkloader iram` the port stays open.

Workaround: none needed without `--speed`. With `--speed`, ignore the first ~5 ms of
output, or keep the `busy_wait_ms(20)` the tests already have before the first print.

Fix: to be found on the host side, in how `tools/bkloader` hands over the baud rate
after `go`.

### F12. Sustained JPEG→RAM DMA crashes the firmware when it runs from IRAM

Files: `src/devices/camera/camera.c` (`camera_stream_*`), `src/linker/iram.lds`,
`tools/bkloader` (`iram`).

Status: cause not found. The failure is tied to the IRAM load path; the same code is stable
when flashed.

Symptom. A firmware image started with `tools/bkloader iram` (code and data in RAM block 2 at
`0x00900000`) that keeps the JPEG encoder DMA running frame after frame fails at random, from
seconds to about 45 s after the stream starts. The same code flashed to the app partition
ran 300 s without a fault (3612 frames, 0 damaged, task and mode stacks far from their
limits, heap without leaks, no orphan or spurious interrupts).

Failure modes seen, all from IRAM, all with the DMA armed:

- `vTaskSwitchContext` assertion;
- the FreeRTOS stack-overflow hook, with stacks that were nowhere near full;
- an undefined-instruction exception inside `printf` code (UND mode);
- silent freezes where the watchdog does not fire;
- bare metal, no timer interrupt, busy-wait loop: 45 s of clean frames (748 frames, 0 dropped,
  0 damaged), then `bk_trap_udef` and a watchdog reset. Registers at the crash:
  `R0 0x000003e8, R1 0xfffffeff, R2 0x00802000, R3 0x008ffef9, R4 0x0090630c,
  R5 0x00400038, R6 0x33, R7 1, R8 0x00904ca9, R9 0x009061b5`. The first FreeRTOS UND had the
  same `R0`/`R1`/`R2`.

Reproduce.

1. Board with the HI704 sensor, own bootloader, a `ram_loader` compatible shell on the chip.
2. Build a test that starts the camera and the stream, for example:
   `camera_start` with `frame_min_bytes = 24 * 1024`, `frame_max_bytes = 47104`, a 64-byte
   `frame_buf`; then `camera_stream_start` with 3 slots of 48 KiB (`aligned(4)`); then loop:
   drain `camera_stream_get` / `camera_stream_release`, `busy_wait_ms(1)`, `wdt_ping()`,
   print counters every 5 s. Platform setup: `wdt_down`, `platform_stdio_init`,
   `wdt_set(10000)`, `wdt_up`, `sctrl_init`, `sctrl_set_cpu_freq_hz(CPU_FREQ_160_MHZ)`,
   enable IRQ and FIQ.
3. Run it from IRAM: `tools/bkloader iram --speed 921600 --port <PORT> --capture 70
   --until "Bootloader*" build/app_iram.bin`. Bare metal fails after roughly 45 s.
4. Replace `busy_wait_ms(1)` with `usleep(1000)` (1 kHz timer interrupt on). It froze before
   the first 5 s line.
5. Under FreeRTOS (`freertos_shell--iram`), run `capture_start`, then `capture_stream 3`
   (or `capture_stream 10`). It breaks within seconds, in a different way on each try.
6. Flash `freertos_shell` instead (`tools/bkloader flash build/app_crc.bin`) and run
   `capture_stream 10`, `capture_stream 300`. Stable.

What the failure needs.

- The DMA must be armed on `rx_fifo_data`. With the encoder running and the DMA off (CPU only
  reading the FIFO in the end-of-frame FIQ) FreeRTOS from IRAM is stable.
- More bus and interrupt load makes it faster: 1 kHz timer interrupt, printing from the FIQ,
  and re-arming the DMA mid-frame from the main loop all shortened the time to failure.

Ruled out.

- DMA overrunning a slot: a 64 KiB guard region after the buffers stayed intact.
- Bit-rate control, WDT task stack size, FreeRTOS port context switch (it fails on bare metal
  too), a slot too small for the frame (this only produces `dropped`, never damage).
- Interrupt storms: no abnormal counts in the interrupt controller raw status at sample times.

Not tried or not possible.

- Memory does not survive a reset, so the state after a crash cannot be read back from the
  bootloader (it fills free RAM with `0xAAAAAAAA`).
- Working theory, never tested directly: instruction fetch from IRAM is unreliable while the
  DMA saturates the bus. IRAM code also runs at about 57 % of the flashed speed, so timing
  differs from the flashed build as well.

Workaround: do not judge long DMA streaming from an IRAM run. Reproduce from a flashed build
first. `capture_stream` from a flashed `freertos_shell` is a valid test.

Fix: none known. Next step would be an UND/prefetch-abort handler that dumps the fault
address and the words around it before the reset, to see whether the code in RAM is
corrupted or the fetch returned garbage.

---

## API / contract

### A4. `flash_read` has no error reporting

File: `hardware_flash/flash.c`.

`flash_read` is `void`-returning and never reports an error.

**Addresses past the end of the flash are deliberately not limited.** The chip uses only
as many address bits as the flash has, so an address past the end wraps: on a 2 MB flash
`0x200000` reads the same bytes as `0x000000`, `0x200010` the same as `0x000010`, and so on
(the address is taken modulo the flash size). A read that starts near the end and runs past
it continues from the start. Nothing fails, and the data returned is real flash content,
only from a different place than the address says.

This is relied on: the `full_4m` segment of `tools/bkloader` reads 4 MB, which on a 2 MB
board returns the same 2 MB twice, with no need to know the board's size in advance.

The same wrap applies to `flash_write` and `flash_erase_sector`. A write or erase past the
end lands at the start of the flash, over the bootloader. Nothing in the firmware limits
that, and no tool in this project sends such an address.

Note: `sr_data_crc_cnt.error_count` was investigated and its function is not
confirmed — it was never observed to increment in any tested scenario. Do not
rely on it as a telemetry counter. See `docs/hardware/flash.md`.

For XIP reads (CPU fetches through the `0x000000–0x1FFFFF` window) the
hardware enforces CRC validation: a mismatch triggers a Data Abort rather
than returning corrupt data silently. This is a hardware-level guard that
applies only to instruction / data cache fetches, not to the OPERATE_SW read
path used by `flash_read`.

Fix: none planned. Reads past the end stay unrestricted.

---

## Cosmetic

### C3. SDK names the IRAM block "PSRAM"

The Beken SDK defines:
```c
// CFG_SOC_NAME == SOC_BK7221U
#define PSRAM_START_ADDRESS  (void*)(0x00900000)
#define PSRAM_END_ADDRESS    (void*)(0x00900000 + 256 * 1024)
```
This is the same 256 KB block that this project calls **IRAM** (RAM block 2, used for
iram-linked firmware images and the `FreeRTOS-TLSF-Heap-All` overflow allocator).
The SDK's `psram_malloc` and the `os_malloc` BK7221U branch that calls it refer to this
block. In our firmware the TLSF adapter already handles IRAM allocation; `os_malloc` in
`port_wifi` is wired directly to `pvPortMalloc` (main heap) without the psram branch.

### C2. RSA driver is a placeholder, no implementation planned

Files: `hardware_security/rsa.c`, `hardware_security/include/hardware/rsa.h`.

`rsa.c` only includes its headers; `rsa.h` is an empty stub with a `TODO`
comment. Register layout for the RSA engine is documented in
`soc/security.h` (`hw_rsa`), but no driver code is planned. Treat the
header as reserved — do not add stubbed functions or sample callers; if
RSA is ever needed, design the API from scratch then.

---

## Architectural follow-ups

These are larger pieces that require a design pass, not a patch.

### Arch2. Uniform error coding across drivers

Today each driver invents its own return scheme:

| Driver | Style |
| --- | --- |
| `efuse` | `-1`, `-2`, `-3` (no names) |
| `gdma`  | named `GDMA_ERROR_*` enum |
| `gpio`  | no error reporting |
| `flash` | no error reporting |
| `timer` | `-1` for everything |
| `aes` / `sha` | `-1` for everything |
| `wdt` | no error reporting |
| `intc` | `bool` |

Pick one of:
- Per-driver named enums (current `gdma` style) for everything.
- A single `enum driver_status { OK = 0, ERR_INVAL = -1, ERR_BUSY = -2, ... }`
  in `soc.h`.

### Arch3. Uniform init lifecycle

Some drivers use `INIT_AT(...)` (gdma, intc, timer), the rest expect the
application to call `<driver>_init()` manually (uart, wdt, flash, gpio,
efuse, sctrl, security). Pick one model; the `INIT_AT` macro plus
`FINI_AT` (already used in `gdma`) is the cleaner option since each driver
declares its own ordering.

### Arch4. Abort mode stack is 16 bytes — custom Data Abort handlers are tightly constrained

`boot_reset.S` initialises `SP_abt` to `_stack_unused` with size
`_UNUSED_STACK_SIZE_ = 0x010` (16 bytes). The same 16-byte region is shared
with UND mode. `boot_dabort` (installed at RAM vector `0x00400014`) pushes
`{r0, r1}` (8 bytes) before branching to the handler slot — leaving 8 bytes
(2 words) of usable abort-mode stack for the handler.

Consequences for custom handlers:

- **Stack overflow at 3+ pushes.** A handler that pushes more than 2
  additional words overflows the abort stack into adjacent memory.
- **Returning to faulting Thumb code requires restoring every clobbered
  register.** AAPCS caller-save rules do not apply here — the "caller" is the
  faulting instruction's register context. Any register modified in the handler
  and not restored before `SUBS PC, LR, #N` will appear with a garbage value
  in the resumed code, which can immediately trigger a second Data Abort (e.g.
  a clobbered register used as a pointer in the next instruction).
- **Return offset differs by instruction set state.** For ARM state:
  `SUBS PC, LR, #4` (skips faulting instruction). For Thumb state:
  `SUBS PC, LR, #6` (Thumb instructions are 2 bytes; `LR_abt = fault + 8`).
  Check `SPSR_abt` bit 5 (T bit) to detect the active state.
- **ARM-only instructions cannot be used in a Thumb naked function.** Write
  the handler in a separate `.S` file with an explicit `.arm` directive.
- **`static` globals are not addressable by name from `.S`.** Remove `static`
  from any file-scope variable the handler needs to write.
- **To call C code, switch to SVC mode first** (which has a properly-sized
  stack). After the switch `LR_abt` / `SPSR_abt` are no longer accessible, so
  returning to the faulting code is not possible — the handler must reboot or
  jump to a known-good address. ARMv5 has no `CPS` instruction:
  ```asm
  mrs  r0, cpsr
  bic  r0, r0, #0x1F
  orr  r0, r0, #0x13   /* SVC mode = 0x13 */
  msr  cpsr_c, r0
  ```

Fix: if a production Data Abort handler is ever needed, increase
`_UNUSED_STACK_SIZE_` in `boot.h` and give ABT mode its own dedicated region
in `flash.lds` / `iram.lds`.

### Arch9. `hardware_pwm` is not wired into the build

`src/platform/drivers/hardware_pwm/` has a complete `CMakeLists.txt`, `pwm.c`,
and `include/` — the library is self-contained and ready to link — but
`src/platform/drivers/CMakeLists.txt` has no `add_subdirectory(hardware_pwm)`
call, so `hardware_pwm` is not part of any CMake configure and no application
can currently link it. `docs/hardware/pwm.md` documents the silicon block, but
nothing in the source tree references the driver.

Fix: add `add_subdirectory(hardware_pwm)` to
`src/platform/drivers/CMakeLists.txt`.

---

## Index of fixed issues

For traceability — these are fixed:

| # | Description                                         |
| --- | ------------------------------------------------- |
| F2 | Timer `ctl` read-modify-write wrote pending W1C `irq_status` flags back as 1 and cleared them; all `enable` changes now go through one helper that writes `irq_status = 0` |
| F3 | `intc_init` no longer unmasks any interrupt source; each driver unmasks its own after registering a handler |
| F4 | `wdt_set` took `unsigned long` and truncated to 16 bits; parameter is now `uint16_t` |
| F5 | `trng_disable` wrote `0x1234` to `data`; the write only latches into the register while disabled and does not change the generated bytes, so it was removed |
| F6 | `trng_enable` delay is measured: with a delay of 16 µs or more the first word differs after every reset; with no delay it repeated in one measurement and not in another. 32 µs kept as margin. `ctrl` has no ready bit |
| Arch7 | `dependencies.cmake` called `FetchContent_Populate()`, deprecated in CMake 3.30+ (CMP0169), and silenced the warning with policy `OLD`. It now uses `FetchContent_MakeAvailable()` with `SOURCE_SUBDIR` pointing at a directory that does not exist, so the dependencies are only downloaded and their own `CMakeLists.txt` is still not loaded; the `*_SOURCE_DIR` variables are unchanged |
| Arch6 | Not a firmware bug: RAM Block 1 is not executable (prefetch abort). Moved to `docs/memory_map.md` (*Not executable*), corrected: RAM Block 2 and the upper SRAM bank are executable, not only IRAM. The flash-build linker script still has no section for code that must run during a flash-bypass window; add one when a driver needs it |
| F7 | Not a firmware bug: the vendor calendar block is absent on this chip. Moved to `docs/hardware/bk7252.md` (*Calendar (not present)*) |
| F10 | `uart_isr()` dequeued with FIQ unmasked, so a FIQ that prints (it reaches the queue through the synchronous path) could rewind the read cursor or overtake the byte the ISR held; the dequeue and the FIFO write now share one section with IRQ and FIQ masked, per byte. Known residue: the FIFO room is counted once before the loop, so bytes a FIQ writes meanwhile could overfill the FIFO; not observed, and no FIQ handler in the tree prints |
| F11 | `bootloader_installer` now resets the chip through the watchdog after a verified install, so the new bootloader starts instead of the stale one in the instruction cache; verified on the chip |
| Arch1 | Multi-field register writes go through `hw_write_fields` (one store). The last three field-by-field sites (`sha` config, `aes` config, `jpeg` ctrl0) were converted; `sha` used to enable the engine before setting its mode. Single-field and shared-register read-modify-write sites (`sctrl`, `random`, `wdt`, the `icu` clock gates in `timer`) are left as they are: they must keep the other bits |
| A1 | `intc_manager` moved from a header into `intc.c`; the header is gone |
| A2 | `intc_register_irq_handler` returned `true` unconditionally and no caller read it; it is now `void` and asserts on a NULL handler, an empty source or a full table. Registering the same handler again adds sources to it, on purpose |
| A3 | Timer `read_ctl` / `read_value` registers do not exist on BK7221U; `timer_read()` and the register fields removed |
| Arch8 | FreeRTOS context save/restore picks the nested path by `SPSR.mode`, so an IRQ before `vPortStartFirstTask()` cannot clobber the first task's frame |
| 1 | `register_handler` left interrupts disabled        |
| 2 | `intc.c` used `|=` to clear W1C status            |
| 3 | `ICU_INT_*_MASK` macros had trailing `;`          |
| 5 | `flash_read` mis-handled unaligned addresses       |
| 6 | `get_absolute_time` torn 64-bit read               |
| 7 | `sha_finish` auto-destroyed the context            |
| 11 | `icu.h` used `inline` instead of `static inline`  |
| 12 | `flash.c` `min` macro replaced with inline fn     |
| 13 | `efuse.c` three RMW ctrl writes → one `.v` write  |
| 14 | `gpio_{get,put,toggle}` lacked range check        |
| 15 | `get_gpio_reg` macro → `static inline`            |
| 16 | `GPIO_HIGH_IMPENDANCE` → `GPIO_HIGH_IMPEDANCE`    |
| Arch5 | `cpu.S`/`cpu.h`/`arm.h` extracted from `platform_boot` into `platform_cpu`; `shell_commands_beken` now explicitly links `platform_cpu` |
| F9 | `flash_init()` added to `hardware_flash/flash.c` (RDID + clk_conf=5 + model_sel=1); registered `INIT_AT(02)`; called explicitly from bootloader `preinit()` |
| C1 | `GPIO_IN_PULLUP`/`GPIO_IN_PULLDOWN` had `GPIO_PULL_MODE_BIT` swapped; confirmed against SDK (`GMODE_INPUT_PULLUP=0x3C`, `GMODE_INPUT_PULLDOWN=0x2C`); fixed in `hardware_gpio/gpio.c`. `GPIO_SECOND_FUNC_PULLUP=0x78` was already correct. |
