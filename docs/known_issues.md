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

### F2. `timer_irq_handler` RMW races against W1C `irq_status`

File: `hardware_timer/timer.c`, function `timer_irq_handler`.

`bank->ctl.enable &= ~(1 << timer_num_in_bank)` (disabling a one-shot timer
from inside the IRQ handler) is a read-modify-write on the *whole* `ctl`
register. The `irq_status` field within that register is write-1-to-clear:
the RMW reads back current `irq_status` bits (1 for any timer whose IRQ has
fired in the meantime), then writes them back as part of the same word — which
silently *clears* the W1C bits. Newly arrived IRQs in the same bank can be
swallowed.

The trailing `do { bank->ctl.irq_status = status0; } while (...)` partially
masks the problem for the bits the handler already saw, but a timer that
fires *after* the snapshot but *before* the disable can lose its flag.

Fix: build the new ctl value with the desired `enable`, the unchanged
`clk_divider`, and `irq_status = 0`, then write through `.v` once. Writing 0
to a W1C bit is a no-op for the hardware, so it does not corrupt other
in-flight flags.

Sketch:
```c
typeof(bank->ctl) tmp = { 0 };
tmp.enable = bank->ctl.enable & ~(1u << n);
tmp.clk_divider = bank->ctl.clk_divider;
bank->ctl.v = tmp.v;
```

### F3. `intc_init` enables MAC FIQ sources before any handler exists

File: `hardware_intc/intc.c`, function `intc_init`.

`INIT_AT(intc_init, 01)` unconditionally enables seven `fiq_*` sources
(`fiq_mac_general`, `fiq_mac_*_trigger`, `fiq_modem`, …). At that moment no
handlers are registered, so any spurious FIQ goes through `intc_fiq()` →
`find_handlers` returns 0 → silent return. If the source is "sticky" the
unhandled IRQ will keep firing in a hot loop.

These bits look like a leftover from the SDK that the camera firmware doesn't
even use. Either:
- delete the seven assignments and let each subsystem `intc_enable_fiq_source`
  itself when ready, or
- move them out of `intc_init` and into a `wifi/mac` init step gated by board /
  feature config.

### F4. `wdt_set(unsigned long)` silently truncates to 16 bits

File: `hardware_wdt/wdt.c` and `include/hardware/wdt.h`.

`void wdt_set(unsigned long period)` stores the argument into
`uint16_t g_period`. All current call sites pass values that fit, but the API
invites a future bug.

Fix: change the parameter type to `uint16_t` in both the header and the
implementation. All current callers (grep'd) already pass small literals.

### F5. `trng_disable` writes the data register

File: `hardware_random/random.c`, function `trng_disable`.

```c
hw_trng->data = 0x1234;
```

`hw_trng->data` is the random-output read register. Writing to it either has
no effect (silent hardware NACK) or some undocumented side effect copied from
SDK boilerplate. Either way it lacks a rationale in code.

Fix: remove the line, or replace with a comment citing the data-sheet section
that requires it.

### F6. `trng_enable` busy-waits a magic 32 µs

File: `hardware_random/random.c`, function `trng_enable`.

```c
busy_wait_us(32); // time to accumulate entropy?
```

The 32 µs figure is a guess (note the question mark). If the TRNG has a
ready / valid status bit (likely sitting in the `ctrl` register's reserved
space), polling that is safer than a magic timeout.

Fix: confirm with the data-sheet whether such a status bit exists; if yes,
poll it; if no, replace the comment with a citation and keep the delay.

### F7. Calendar block (`beken378/driver/calendar/`) absent on BK7221U

The Beken SDK ships a `calendar` driver (`calendar.c` / `calendar.h`,
base address `0x00800000 + 0x55*4 = 0x00800154`) described as a 32 kHz
free-running counter that exposes `cal_get_time_us()`.

Verified on hardware: the registers at `0x00800154–0x00800160` are unmapped
on BK7221U — reads return 0, writes are silently ignored regardless of
`SCTRL_BLOCK_EN_MUX` or `SCTRL_LOW_PWR_CLK` configuration. The block is
not mentioned in `BK7252_Data_Sheet_V1.0.pdf`.

In the SDK the calendar is only used when both `CFG_USE_TICK_CAL=1` and
`CFG_LOW_VOLTAGE_PS=1` (low-voltage sleep tick calibration), a combination
that is not used in this project. The SDK's `#if !(CFG_SOC_NAME == SOC_BK7252N)`
guard around `cal_get_time_us()` in `os_clock.c` appears to be a copy-paste
from other chip variants where the block does exist.

Use `get_absolute_time()` from `hardware_time` (NXMAC monotonic counter at
`0xC000011C`) as a drop-in replacement — it provides microsecond resolution
and is confirmed working on this chip.

### F8. `platform_stdio_init()` requires a settle delay before first TX

File: `src/platform/misc/stdio/stdio.c`.

Output written immediately after `platform_stdio_init()` is transmitted with the wrong
bit timing and arrives at the host as garbage. The delay is still required; what it
guards against is corruption, not a crash.

Measured during hardware probing (`src/tests/probe/`, August 2026):

| Delay after `platform_stdio_init()` | Result |
|---|---|
| 0 ms | garbled output; the chip survives and keeps running |
| ≤ 5 ms | garbled output |
| ≥ 6 ms | clean output |
| 20 ms | clean output (chosen as safe default with margin) |

The threshold was pinned by emitting one labelled 6-byte line per millisecond starting
at 0 ms and reading off the first legible label: `T=00`..`T=05` arrived corrupted,
`T=06` onward clean, with a worst-case emit drift of 2 µs against the intended
schedule. Single run at 1 ms granularity — treat 6 ms as the observed floor, not a
qualified minimum.

The corruption depends on elapsed time since init, not on how many bytes have been
sent: after a 20 ms delay output is clean from the very first byte.

No pollable readiness signal exists, so the blind delay cannot be replaced by polling.
Sampled immediately after `uart2_init()` and repeatedly over the following
microseconds, `fifo_status` already reads `0x001a0000` — `tx_empty`, `rx_empty` and
`wr_ready` all set, `tx_fifo_count` zero — and `config` already holds the correct
divisor (`0xe1` = 225 for 115200 from the 26 MHz XTAL). Every register the UART block
exposes reports ready while transmission is still corrupt, which places the cause in
the clock domain (ICU gating / clock switch), not in the UART.

Two earlier descriptions of this entry were wrong and are corrected above:

- The failure was recorded as a hard fault with a register dump and bootloader
  takeover. It is not — the chip neither faults nor resets. The dump previously
  attributed to F8 was most likely the deliberate `wdt_reboot()` at the end of a probe.
- The observation that "10 ms produced no output" was a host-side artefact of the old
  `tio` workflow: the loader closed the serial port, discarding the OS receive buffer,
  before `tio` opened it. The bytes were transmitted correctly but arrived during the
  port-close gap. With `--capture` on `bkloader iram` the port stays open.

Workaround: `busy_wait_ms(20)` between `platform_stdio_init()` and the first
I/O call. Used in `src/tests/probe/probe.cpp`.

Fix: none available inside the UART block — see the readiness-signal paragraph above.
Reducing the delay means characterising the clock switch in `icu_uartN_clk()` /
`icu_uartN_power_up()`, or accepting the measured 6 ms floor with margin.

---

## API / contract

### A1. `intc_manager` is `static` in a header

File: `hardware_intc/intc_manager.h`.

```c
static struct { ... } intc_manager;
```

Defining `static` storage in a header means every translation unit that
includes it gets its own copy. Today only `intc.c` includes the header so the
hazard is dormant, but adding any second user (a test, a shell command that
inspects the table, a debug dump) would silently get an empty `intc_manager`
and break `intc_register_*` from outside `intc.c`.

Fix: declare `extern` in the header, define once in `intc.c`. While at it,
consider moving `intc_manager.h` content into the `.c` since nothing else
needs it.

### A2. `intc_register_*_handler` return value is uninformative

File: `hardware_intc/intc.c`, also `intc_manager.h::register_handler`.

`register_handler` returns `true` unconditionally; the only way it ever
returns `false` is the explicit `count >= MAX_HANDLERS` check in
`intc_register_irq_handler` / `intc_register_fiq_handler`. Bad input
(`func == NULL`, `source == 0`, duplicate registration) is silently accepted.

Fix: validate inputs in `register_handler` itself and return `false` on
unrecognised cases. Update the callers to surface the failure (currently the
return is mostly ignored).

### A3. Timer counter read-back registers absent on BK7221U — resolved

The `read_ctl` and `read_value` registers described in some Beken documentation
do not exist on BK7221U silicon. The SDK `beken378/driver/pwm/bk_timer.h`
explicitly excludes them:

```c
#if (CFG_SOC_NAME != SOC_BK7221U)
#define TIMER0_2_READ_CTL    (PWM_NEW_BASE + 4 * 4)
#define TIMER0_2_READ_VALUE  (PWM_NEW_BASE + 5 * 4)
#endif
```

Both timer banks are affected. Reading the current counter value mid-period is
not possible on this chip. `timer_read()` has been removed from the driver and
public API. `soc/timer.h` no longer declares `read_ctl` / `read_value` fields.

### A4. `flash_read` has no error reporting

File: `hardware_flash/flash.c`.

`flash_read` is `void`-returning. It silently succeeds with garbage if:
- `addr + count` walks past the end of the flash (no upper bound check),
- the flash is in deep-power-down mode.

Note: `sr_data_crc_cnt.error_count` was investigated and its function is not
confirmed — it was never observed to increment in any tested scenario. Do not
rely on it as a telemetry counter. See `docs/hardware/flash.md`.

For XIP reads (CPU fetches through the `0x000000–0x1FFFFF` window) the
hardware enforces CRC validation: a mismatch triggers a Data Abort rather
than returning corrupt data silently. This is a hardware-level guard that
applies only to instruction / data cache fetches, not to the OPERATE_SW read
path used by `flash_read`.

Fix: return `int` with a meaningful error code, validate `addr + count`.

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

### Arch1. Audit remaining register-write call sites

The `hw_write_fields` helper in `soc.h` is the project-wide idiom
for atomic multi-field MMIO writes. Known multi-field call sites (`efuse`,
`gdma`, `intc::intc_init`, `uart::uart_init`) have been converted. Future
drivers should follow the same shape; existing single-bit RMW writes
(`sctrl`, `random`, `wdt`'s `wdt_up`/`wdt_down` toggle) are correct as-is
— they intentionally preserve the other fields and should NOT be rewritten
through the helper.

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

### Arch6. RAM (0x00400000) is non-executable — `__data` / `.data` code causes prefetch abort

Confirmed on hardware via `src/tests/probe/` (June 2026).

The BK7252 MPU marks the general-purpose RAM region (`0x00400000–0x004FFFFF`) as
non-executable.  Any attempt to branch to an address in this region immediately
triggers a prefetch abort.  Only IRAM (`0x00900000–0x0093FFFF`) is executable.

Implications:

- The `__data` attribute (or `__attribute__((section(".data")))`) places the function
  body in the `.data` section, which is linked to RAM VMA.  Code compiled with this
  attribute will crash with a prefetch abort the first time it is called.
- For flash firmware, code that must run during the XIP gap (e.g. `flash_bypass_xfer`
  while `flash_spi_mux = 1`) **must** reside in IRAM.  Use
  `__attribute__((section(".iram.text")))` or a dedicated IRAM section.
- For IRAM firmware (`iram.lds`), all `.text` is already in IRAM, so this constraint
  is automatically satisfied — no special attributes are needed.

Workaround: for the bypass driver (see `wip/flash-bypass` branch), IRAM firmware is
used for now.  A production flash-firmware version of `flash_bypass_xfer` will need an
explicit IRAM section attribute.

### Arch7. `FetchContent_Populate` deprecated in CMake 3.30+ (CMP0169)

CMake 4.2 (shipped with CLion 2026.1.3) emits a deprecation warning for the
direct `FetchContent_Populate()` calls in `dependencies.cmake` — all three
dependencies (FreeRTOS-Kernel, lwIP, tlsf) use the old populate-only pattern.

The modern replacement is `FetchContent_MakeAvailable`, which also calls
`add_subdirectory` on the fetched content. The current code intentionally
skips that step — each dependency is integrated manually via its source-dir
variable (`FREERTOS_KERNEL_PATH`, `LWIP_DIR`, tlsf source) rather than
through CMake's own targets.

Migration path:
1. Verify that the FreeRTOS-Kernel V11.1.0 `CMakeLists.txt` can be loaded via
   `FetchContent_MakeAvailable` without conflicting with the port's manual
   integration (setting `FREERTOS_KERNEL_PORT` will be required).
2. Do the same for lwIP (requires the port's `lwipopts.h` to be on the
   include path before `add_subdirectory`).
3. tlsf has a trivial `CMakeLists.txt`; a thin wrapper target may suffice.

Until the migration is done, set `cmake_policy(SET CMP0169 OLD)` in
`dependencies.cmake` (guarded with `if(POLICY CMP0169)`) to suppress the
warning.

### Arch8. FreeRTOS port assumes no IRQ can land before `vPortStartFirstTask()` runs — resolved

Files: `platform/freertos/port.c` (`xPortStartScheduler`), `platform/freertos/port_asm.S`
(`portSAVE_CONTEXT` / `portRESTORE_CONTEXT`).

Confirmed on hardware (`freertos_shell--iram`, July 2026): jumping into the app from
the bootloader intermittently landed back at address `0x0` (the bootloader restarting
itself) with no watchdog reset involved. Root cause traced to a corrupted
`pxCurrentTCB` stack frame, not a hardware or bootloader/vector-handoff issue.

Mechanism:

- `portSAVE_CONTEXT` / `portRESTORE_CONTEXT` (used by `do_irq`) always save/restore
  relative to `pxCurrentTCB`, on the assumption that the CPU is genuinely executing
  *inside* that task's context whenever an IRQ is taken.
- `pxCurrentTCB` is already valid before `vTaskStartScheduler()` ever calls into
  `xPortStartScheduler()` — FreeRTOS assigns it to the first created task at
  `xTaskCreateStatic()` time.
- If an IRQ is taken anywhere between task creation and `vPortStartFirstTask()`'s
  final `subs pc, lr, #4` (which atomically restores the task's SPSR into CPSR —
  unmasking IRQ — and jumps into the task, in one instruction), the CPU is not
  actually inside any task yet; it is still on `xPortStartScheduler()`'s own C call
  stack. `portSAVE_CONTEXT` nonetheless writes that wrongly-shaped stack content into
  `pxCurrentTCB`'s saved-stack-pointer field, clobbering the first task's properly
  initialised `pxPortInitialiseStack()` frame.
- The next restore (tick-driven or otherwise) reads that garbage back as
  `{R0-R14, SPSR, ulCriticalNesting, LR}` and branches to whatever garbage value
  lands in the "return address" slot — observed as literal `0`: an ordinary, valid
  branch into the bootloader's still-resident vector table (`b _reset`), not a fault
  or a watchdog reset. This also explains why the CPSR/register dump printed by the
  bootloader's "go" command on the failing run looked like plausible-but-wrong data
  (e.g. `CPSR: 0x00000010`) rather than an all-zero or clearly-invalid pattern — it's
  the same corrupted stack data being reinterpreted as a register frame.

This was safe only because nothing was ever ICU-forwarded early enough to test the
invariant: the design relied on CPU IRQ (CPSR I-bit) staying masked continuously from
`_reset` until `vPortStartFirstTask()`'s SPSR-restore trick unmasks it, and no
interrupt source was live during that window until commit `01e2875` made UART2's
`rx_need_read` / `rx_stop_end` live at the ICU level for the first time
(`uart2_init()`). Since the console UART is actively receiving bytes throughout
normal use (typing at the shell), a byte landing at the wrong instant reliably
reproduced the corruption. The immediate fix for that regression — reverting the
UART1/UART2 interrupt-driven TX wait — only removed the live trigger.

Resolution: the underlying hole is now closed, by ARM processor mode rather than by a
software flag. `portSAVE_CONTEXT` (commit `ac8788b`) tests `SPSR.mode` on entry and
takes the `nested_save` branch whenever the interrupted context was not System mode:
the frame goes on the current mode's own stack and `pxCurrentTCB` is left untouched.
`portRESTORE_CONTEXT` mirrors the same test, and `portAPPLY_PENDING_SWITCH` (commit
`2c8786d`) only calls `vTaskSwitchContext()` when `SPSR` says System mode. Tasks run in
System mode; everything from `_reset` up to `vPortStartFirstTask()` runs in Supervisor
mode, so an IRQ taken in the vulnerable window now takes the nested path and cannot
clobber the first task's `pxPortInitialiseStack()` frame.

That is functionally option (a) below, with the processor mode standing in for the
`xSchedulerRunning` flag — and it is strictly better, because it needs no flag to be
maintained and covers nested handlers as well as the pre-scheduler window.

The guard above was established by reading `port_asm.S`; the pre-scheduler scenario has
not been re-run on a FreeRTOS target since. That is a gap in the evidence, not a live
risk, because nothing currently reaches the window:

- Reintroducing interrupt-driven UART TX (`hardware_uart/uart.c`) does not open it.
  `_write()` arms `tx_need_write` but then checks `portENABLED_IRQ()`, which reads the
  CPSR I-bit — masked continuously from `_reset` until `vPortStartFirstTask()`. So every
  pre-scheduler write takes the synchronous path, and `uart_drain()` clears the enable
  bit and releases the byte source before returning. By the time IRQ is unmasked there
  is nothing left to assert. The same holds for `bk_printf()` from a MAC handler, since
  IRQ/FIQ entry masks I as well.
- `freertos_wifi` has since been built and booted with this in place (August 2026),
  which exercises exactly that early-`printf` path.

The general warning still stands, though: any future source that is ICU-forwarded and
left unmasked at the block during the window can still take an IRQ there, and would then
be relying on the mode guard rather than on nothing being live. Confirming the guard on
hardware is worth doing before that happens.

Fix options as originally scoped, kept for context:
- (a) Guard `do_irq`'s context switch on an `xSchedulerRunning`-equivalent flag, so an
  IRQ taken before the scheduler has truly started is acknowledged/serviced without
  touching `pxCurrentTCB` (matches how upstream FreeRTOS ARM7/9 ports guard
  tick-driven switches). — adopted, in the mode-based form described above.
- (b) Audit every driver's early `intc_enable_*_source()` / `INIT_AT` call site and
  guarantee none of them can be ICU-forwarded before `vPortStartFirstTask()` runs.
- (c) Move all peripheral interrupt enabling out of pre-`main()` init and into a
  first, highest-priority task that runs after the scheduler has started, so nothing
  can be live during the vulnerable window at all.

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

For traceability — these were fixed in commit `a7cebc0`:

| # | Description                                         |
| --- | ------------------------------------------------- |
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
