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
   ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
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
    busy_wait_ms(20);         // UART settle — 0 ms causes hard fault (see F8 in drivers_known_issues.md)
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
| `Port … not found` | Device not connected or wrong port | Check USB cable; re-run `ls /dev/ttyUSB* /dev/ttyACM*` |
| `Timed out waiting for shell prompt` | Chip hung or wrong firmware flashed | Power-cycle the board, or briefly connect CEN to GND |
| `Failed to switch baud rate` | Firmware missing `speed` command | Use without `--speed`, or flash `ram_loader` |
| `Permission denied` on `/dev/tty*` | User not in the serial port group | `sudo usermod -aG dialout $USER` (Ubuntu/Debian) or `uucp` (Arch), then re-login |

## Adding drivers to an experiment

Find available drivers under `src/platform/drivers/` — each subdirectory is one library (`hardware_<block>`). Public headers are in `include/hardware/<block>.h` inside that directory.

To use a driver: include its header in `probe.cpp` and add the library name to `target_link_libraries` in `src/tests/probe/CMakeLists.txt`.
