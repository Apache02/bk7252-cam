# BK7252 Watchdog timer

Single watchdog at `0x00802900`. When the countdown expires it resets the whole chip.
The block has one 32-bit register. It is clocked from the low-power clock, so it counts
independently of the CPU clock, and its clock gate sits in the ICU, not in the block.

---

## Register map

Offset in bytes from `0x00802900`.

| Offset | Name   | Purpose                                         |
| ------ | ------ | ----------------------------------------------- |
| `0x00` | `ctrl` | Period and reload key; both fields readable     |

### `ctrl`

| Bits      | Name     | Purpose                                                       |
| --------- | -------- | ------------------------------------------------------------- |
| `[15:0]`  | `period` | Timeout in units of 1 ms                                      |
| `[23:16]` | `key`    | Reload key: `0x5A` then `0xA5`                                |
| `[31:24]` | reserved |                                                               |

### Clock gate

The block is enabled by the ICU peripheral clock power-down register, bit `[8]`
(`arm_wdt`). `0` = clock running, watchdog counts. `1` = clock gated, watchdog stopped.

The ICU clock gate-disable register has a separate `wdt` bit, `[9]` (`1` = gate disabled,
clock always on). It reads `1` after reset. **It does not gate the watchdog clock.**
With `arm_wdt = 0`, the watchdog counts and resets the chip after the same time with the
bit at `0` or `1` (2003 ms at `period = 2000`). Flipping the bit while the countdown runs
(to the opposite value at 500 ms, back at 1500 ms) does not delay the reset: 2002 ms
starting from `1`, 2003 ms starting from `0`. `ctrl` writes are accepted at either value.
Clearing the register's `icu` bit `[0]` together with `wdt` does not stop the watchdog
either: 2003 ms.
Whether the bit matters in a low-power state is not characterised; leave it at `1`. The `tl410_wdt` bits in the ICU belong to the TL410 core's own watchdog, not to
this block.

`ctrl` is writable and readable while `arm_wdt = 1` (clock stopped), in both states of
the gate-disable bit.

---

## Operation model

The block holds one period value and one countdown. Reaching zero resets the chip.
A reload restarts the countdown from the period.

**The period is not stored separately from the reload write.** Every reload write carries
the period in `[15:0]`, and the written value becomes the new period. A reload that
writes `0` in `period` sets the period to `0`.

**`ctrl` reads back the last value written.** `period` reads as the last written period
and does not count down. The countdown itself is not visible.

### Period unit

The timeout equals `period` milliseconds, to within the measurement resolution of 1 ms.
Measured from the reload write to the chip reset, against a 26 MHz timer:

| `period` | Time to reset |
| -------- | ------------- |
| `2000`   | 2003 ms       |
| `10000`  | 10002 ms      |

The offset of 2–3 ms is the same at both periods, so the unit is linear. The reload write
and the clock-gate enable were issued back-to-back, so the start of the countdown is
only known to within that interval. A 16-bit field gives a longest period of 65535 ms
by the same unit; not measured.

Reset value of `ctrl` on a cold power-on reset: `0x00004000` (`period = 0x4000`).

---

## Operation sequence

1. Set the ICU `arm_wdt` bit to `1` to stop the watchdog while configuring.
2. Write `ctrl` with `key = 0x5A` and the desired `period`.
3. Write `ctrl` with `key = 0xA5` and the same `period`.
4. Set the ICU `arm_wdt` bit to `0` to start the watchdog.
5. To postpone the reset, repeat steps 2–3 before `period` ms elapse. Read the current
   period from `ctrl[15:0]` and write it back to keep it unchanged.

---

## Notes

- **A write with a wrong key still changes `period`.** Writing `key = 0x00`,
  `period = 777` makes `period` read back `777` and `key` read back `0x00`. Whether the
  countdown restarts on such a write is not characterised. Always write the two-key
  sequence.
- After a correct two-key sequence `key` reads back `0xA5`.
- The countdown value is not readable. Time to expiry cannot be polled.
- Behaviour with `period = 0` is not characterised.
- A watchdog reset resets the full chip but does not power-cycle the analog blocks; see
  `docs/por_reset_state.md`.
