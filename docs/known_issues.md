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

File: `src/platform/stdio/stdio.c`.

Calling `setvbuf` or `printf` immediately after `platform_stdio_init()` (0 ms
gap) produces a hard fault — register dump is visible on UART, then the
bootloader takes over. With a delay of ≥ 10 ms, UART output is reliable.

Observed during hardware probing (`src/tests/probe/`):

| Delay after `platform_stdio_init()` | Result |
|---|---|
| 0 ms | hard fault (register dump, bootloader recovery) |
| 10 ms | reliable output |
| 20 ms | reliable output (chosen as safe default with margin) |

The exact minimum was not pinned below 10 ms. The root cause is likely that
the UART peripheral clock or baud-rate divisor needs time to stabilise after
the register write in `stdio_init` — the first character arrives before the
UART TX shift register is ready.

Note: the earlier observation that "10 ms produced no output" was a host-side
artefact of the old `tio` workflow: `iram_loader` closed the serial port,
discarding the OS receive buffer, before `tio` opened it. The bytes were
transmitted correctly but arrived during the port-close gap. With the
`--capture` flag on `iram_loader` (port stays open), 10 ms is confirmed
reliable.

Workaround: `busy_wait_ms(20)` between `platform_stdio_init()` and the first
I/O call. Used in `src/tests/probe/probe.cpp`.

Fix: investigate whether a UART ready/busy status bit exists in the UART
peripheral registers that can be polled instead of using a blind delay.

### F9. Bootloader missing `flash_init` — `JEDEC_ID` reads zero

File: `src/applications/bootloader/main.cpp`, `preinit()`.

The bootloader's `preinit()` calls `sctrl_init()` and `uart2_init()` but never
calls `flash_init()`. As a result `hw_flash->JEDEC_ID` returns `0x000000` and
all flash commands (`flash_dump`, `flash_read`, `flash_write`, `flash_crc32`,
`partitions`) operate on uninitialised hardware.

The `freertos_shell` application works because `flash_init` is wired up via
`INIT_AT` in `hardware_flash/flash.c` and runs automatically through
`__libc_init_array`. The bootloader uses `PREINIT_AT` for its own `preinit`,
which runs before `__init_array` — so `flash_init` has not been called yet when
`preinit` executes.

Fix: add `flash_init()` to `preinit()` in `bootloader/main.cpp`, after
`sctrl_init()` and before any flash command is reachable.

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

### A3. `timer_read` is marked "not working" in source

File: `hardware_timer/timer.c`, function `timer_read`.

```c
// not working
int timer_read(int timer_num) { ... }
```

The function is exported in `include/hardware/timer.h` and called from
`src/applications/shell/commands/timers_test.cpp:41`. Either:
- fix the read-back sequence (the `read_op` / `read_index` / `read_value`
  handshake; investigate whether the `read_value` needs a particular alignment
  or whether the bank's `clk_divider` masks the read), or
- remove the function from the public header and from `timers_test.cpp`.

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

### C1. `GPIO_SECOND_FUNC_PULLUP` is named "pullup" but configures pulldown

File: `hardware_gpio/include/hardware/gpio.h` and `hardware_gpio/gpio.c`.

The enum sets `GPIO_PULL_MODE_BIT = 1` in addition to `GPIO_PULL_ENABLE_BIT`.
By the convention used elsewhere in the same driver (`GPIO_IN_PULLUP` =
`pull_mode = 0`, `GPIO_IN_PULLDOWN` = `pull_mode = 1`) this combination is
pulldown, not pullup.

It is currently used only for UART RX pins (where pulldown is wrong on its
own, but the second-function override may make it irrelevant), so the actual
electrical behaviour might be correct by accident. Worth verifying with a
scope before renaming, because:
- if behaviour is actually pulldown but desired is pullup, the bit is wrong;
- if behaviour is pullup, the BK convention for `pull_mode` differs from the
  comments in this file.

Action: either rename the enum to `GPIO_SECOND_FUNC_PULLDOWN` or flip the
`GPIO_PULL_MODE_BIT` value to match the name — but first measure on hardware
to know which side of the contradiction to trust.

### C2. RSA driver is a placeholder, no implementation planned

Files: `hardware_security/rsa.c`, `hardware_security/include/hardware/rsa.h`.

`rsa.c` only includes its headers; `rsa.h` is an empty stub with a `TODO`
comment. Register layout for the RSA engine is documented in
`security_regs.h` (`hw_rsa`), but no driver code is planned. Treat the
header as reserved — do not add stubbed functions or sample callers; if
RSA is ever needed, design the API from scratch then.

---

## Architectural follow-ups

These are larger pieces that require a design pass, not a patch.

### Arch1. Audit remaining register-write call sites

The `hw_write_fields` helper in `register_defs.h` is the project-wide idiom
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
  in `register_defs.h`.

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
| 9 | `g_sys_counter` was not `volatile`                 |
| 11 | `icu.h` used `inline` instead of `static inline`  |
| 12 | `flash.c` `min` macro replaced with inline fn     |
| 13 | `efuse.c` three RMW ctrl writes → one `.v` write  |
| 14 | `gpio_{get,put,toggle}` lacked range check        |
| 15 | `get_gpio_reg` macro → `static inline`            |
| 16 | `GPIO_HIGH_IMPENDANCE` → `GPIO_HIGH_IMPEDANCE`    |
| 23 | `register_sys_counter` had no IRQ-disable guard   |
| Arch5 | `cpu.S`/`cpu.h`/`arm.h` extracted from `platform_boot` into `platform_cpu`; `shell_commands_beken` now explicitly links `platform_cpu` |
