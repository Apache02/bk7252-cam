---
name: run-on-chip
description: Use when testing a hardware hypothesis on the real BK7252 chip — register behaviour, peripheral interaction, driver correctness. Triggers on "test this on chip", "verify on hardware", "check what this register does", "probe this peripheral", "let me see on real hardware", or any task requiring autonomous write→build→flash→capture→iterate in src/tests/probe/.
---

# Run-on-Chip

Autonomous hardware testing loop for the BK7252 via `src/tests/probe/`. Use this to confirm or disprove hardware hypotheses before writing documentation or proper driver tests.

## Scope

| Zone | Autonomous | Notes |
|---|---|---|
| `src/tests/probe/` | Write, build, flash, iterate | — |
| `src/tests/<name>/` | Write, build, flash | Ask before starting |
| `src/applications/` | Write code | — |
| `src/applications/` build/flash | ❌ never | — |
| `docs/` | — | Explicit command required |

## Prerequisite

`tools/iram_loader` requires a shell with `loadi`, `go`, and `speed` commands running on the chip. `src/applications/ram_loader` is the dedicated app for this, but any firmware with these shell commands works. If the loader fails to connect, ask the user to flash a compatible app first.

## Probe Cycle (max 5 iterations)

1. **State the hypothesis** — one sentence: what you expect and why.
2. **Write `src/tests/probe/probe.cpp`** — rewrite completely; add drivers to `CMakeLists.txt` as needed.
3. **Build:**
   ```sh
   (cd build && make test_probe--iram)
   ```
   If build fails: read errors, fix `probe.cpp` / `CMakeLists.txt`, retry (counts as one iteration).
4. **Flash and capture:**
   Find the port:
   ```sh
   find /dev -maxdepth 1 \( -name 'ttyUSB*' -o -name 'ttyACM*' \) | sort
   ```
   Port selection rules:
   - Single port found → use it.
   - Multiple `ttyACM*` ports → likely the RP2040 bridge: use `ttyACM1` (BK7252 UART). `ttyACM0` is the RP2040 shell — do not use it.
   - Multiple `ttyUSB*` or ambiguous mix → ask the user which port to use.

   Then:
   ```sh
   python3 tools/iram_loader --speed 921600 --port <PORT> \
       --capture 10 --until "==END==" build/app_iram.bin
   ```
5. **Interpret output:**
   - All lines `[ OK ]` and summary shows `N / N passed` → hypothesis confirmed, go to step 6.
   - Any `[FAIL]` or no sentinel → revise hypothesis, return to step 1.
   - After 5 iterations without resolution → stop, report findings to user.

6. **After confirmation** — ask the user:
   - Whether to write a proper test to `src/tests/<name>/` (ask before starting, then proceed autonomously)
   - Whether to write documentation to `docs/hardware/<block>.md` (wait for explicit command)

## probe.cpp invariants

Always uphold these — every experiment, no exceptions:

```cpp
int main() {
    wdt_down();               // FIRST — disables watchdog immediately
    platform_stdio_init();
    busy_wait_ms(20);         // UART settle — 0 ms causes hard fault (see F8 in known_issues.md)
    setvbuf(stdout, NULL, _IONBF, 0);  // unbuffered — output visible even on crash

    wdt_set(10000);           // set 10 s period
    wdt_up();                 // start watchdog — wdt_set alone does not activate it

    // --- experiment code ---

    printf("\r\n%u / %u passed\r\n", g_passed, g_total);
    printf("==END==\r\n");   // sentinel — iram_loader stops capture here
    wdt_reboot(100);          // clean reset — chip ready for next flash
    return 0;
}
```

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `Port … is busy` | tio or another terminal has the port open | Close tio / terminal, retry |
| `Port … not found` | Device not connected or wrong port | Check USB cable; re-run `find /dev -maxdepth 1 \( -name 'ttyUSB*' -o -name 'ttyACM*' \)` |
| `Timed out waiting for shell prompt` | Chip hung or wrong firmware flashed | Power-cycle the board, or briefly connect CEN to GND |
| `Failed to switch baud rate` | Firmware missing `speed` command | Use without `--speed`, or flash `ram_loader` |
| `Permission denied` on `/dev/tty*` | User not in the serial port group | `sudo usermod -aG dialout $USER` (Ubuntu/Debian) or `uucp` (Arch), then re-login |

## Adding drivers to an experiment

Find available drivers under `src/platform/drivers/` — each subdirectory is one library (`hardware_<block>`). Public headers are in `include/hardware/<block>.h` inside that directory.

To use a driver: include its header in `probe.cpp` and add the library name to `target_link_libraries` in `src/tests/probe/CMakeLists.txt`.

## ARM-mode assembly (`probe_arm.S`)

`src/tests/probe/probe_arm.S` is a permanent fixture — always compiled and linked. Use it for any code that must run in ARM mode (not Thumb), such as exception handlers, `SUBS PC, LR, #offset` returns, or code that cannot be expressed in C/Thumb.

- The file uses `.arm` — all functions in it are ARM-mode regardless of the gcc default.
- Declare symbols in `probe.cpp` as `extern "C"` and reference them normally.
- When starting a fresh experiment that doesn't need ARM code: leave the file with a minimal stub (at least one exported symbol so the linker doesn't warn).
- When the experiment needs an exception handler or other ARM-mode routine: write it here.

Key facts for ARM exception handlers on this chip (ARM968E-S, ARMv5TE):

| Exception | LR_mode (ARM state fault) | LR_mode (Thumb state fault) | Return to next instruction | Confirmed |
|---|---|---|---|---|
| Undefined instruction | `fault_addr + 4` | `fault_addr + 2` | `MOVS PC, LR` | **hardware** |
| Prefetch Abort | `fault_addr + 4` | `fault_addr + 4` | `MOVS PC, LR` | spec only |
| Data Abort | `fault_addr + 8` | `fault_addr + 6` | `SUBS PC, LR, #4` | spec only |

Hardware-confirmed UNDEFINED details (probe result, ARM state, fault at `0x000F0000`):
- `LR_und = 0x000F0004` = `fault_addr + 4` ✓
- `SPSR_und = 0x000000D3` = SVC mode, ARM state, IRQ+FIQ masked
- Return to next instruction: `MOVS PC, LR`

Data Abort hookability from IRAM context: the bootloader's DABORT handler at `0x00000628` does read `hook_dabort` at `0x00400014` (confirmed by disassembly of `bootloaders/bootloader_a9_v720.bin`). However, installing a handler via `hook_dabort` and triggering `0x00200000` (flash mirror CRC abort) did not route to the installed handler in repeated tests from IRAM. Root cause not resolved. Data Abort hooks may work from flashed firmware but are unreliable from IRAM.

Mode switch without CPS (ARMv5, no CPS instruction):
```asm
mrs  r0, cpsr
bic  r0, r0, #0x1F
orr  r0, r0, #0x13   /* 0x13 = SVC, 0x1F = SYS, 0x12 = IRQ, 0x11 = FIQ, 0x17 = ABT */
msr  cpsr_c, r0
```

ABT and UND mode stacks are initialised. Both share the 16-byte UNUSED stack at `0x43E030`–`0x43E03F` (SP = `0x43E040`). The bootloader's dispatch code pushes `{r0, r1}` (8 bytes) before calling the installed handler, so the effective usable space in the handler itself is 8 bytes. Keep handlers register-only (no push/pop) unless you switch to a larger stack first.

UART FIFO drain before triggering exceptions: `printf()` leaves bytes queued in the UART TX FIFO. If an exception handler writes to UART immediately after, the FIFO may be full and writes are silently dropped. Always call `busy_wait_ms(50)` after the last `printf` and before the fault trigger, and poll `wr_ready` (bit 20 of `fifo_status`) inside the handler before each byte write.
