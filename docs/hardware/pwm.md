# BK7252 PWM / Timer block

6-channel output-compare unit at `0x00802A80` (`PWM_BASE_ADDR`). Each channel
can operate as a PWM signal generator, a periodic interrupt timer, or an edge
capture input. The same counter hardware is shared between all three modes.

---

## Address map

All registers are 32-bit, word-aligned.

| Offset | Name         | Description                               |
|--------|--------------|-------------------------------------------|
| `0x00` | `ctl`        | Per-channel enable / int-enable / mode (4 bits × 6 channels = 24 bits) |
| `0x04` | `int_status` | Per-channel interrupt flags [5:0]; W1C    |
| `0x08` | `ch[0].period` | Channel 0 period (counter wraps here)   |
| `0x0C` | `ch[0].duty`   | Channel 0 duty threshold                |
| `0x10` | `ch[0].cap`    | Channel 0 capture latch (capture modes) |
| `0x14` | `ch[1].period` | …                                       |
| …      | …              | Channels 1–5 follow at `+3 words` each |

### `ctl` register — bit layout

Channel `n` occupies bits `[4n+3 : 4n]`:

| Bits     | Field      | Description                                  |
|----------|------------|----------------------------------------------|
| `4n+0`   | `en`       | 1 = channel running                          |
| `4n+1`   | `int_en`   | 1 = assert IRQ when counter wraps            |
| `4n+3:4n+2` | `mode` | 0 = PWM output, 1 = timer, 2 = cap pos edge, 3 = cap neg edge |

### `period` / `duty` / `cap`

- **`period`**: counter counts from 0 upward and wraps back to 0 when it reaches
  this value. IRQ fires on wrap if `int_en = 1`.
- **`duty`**: in PWM output mode, pin is high while `counter < duty`, low while
  `counter ≥ duty`. `duty = 0` → pin always low; `duty ≥ period` → pin always
  high.
- **`duty` can be updated on-the-fly** while the channel is running — the new
  value takes effect on the next counter wrap (confirmed in silicon).
- **`cap`**: in capture mode, latches the current counter value on the selected
  edge. Read-only.

---

## Clock configuration (ICU)

Two ICU registers must be set before starting any channel.

### 1. Enable peripheral clock — `peri_clk_pwd`

PWM channels 0–5 occupy bits `[14:9]` of `peri_clk_pwd`, one bit per channel.
Clear a channel's bit to `0` to enable its clock (`0` = clock running, `1` =
powered down).

### 2. Select clock source — `pwm_clk_mux`

One bit per channel; bits `[5:0]` correspond to channels 0–5.

| Value | Source         |
|-------|----------------|
| `0`   | PCLK (~26 MHz) |
| `1`   | LPO (32 kHz)   |

Default at power-on is PCLK (`0`).

### Period-to-frequency formula

With PCLK and no divider (the PWM block has no independent prescaler):

```
period = PCLK_Hz / desired_frequency_Hz
```

Example: `period = 26000` → 1 kHz at 26 MHz PCLK.

---

## GPIO pin mapping

The PWM block does not have a configurable output-pin mux — each channel maps
to one fixed GPIO pin. Two register writes are required to connect a channel
to its pin:

**Step 1 — write the `FUNC_CFG` register** (selects which peripheral drives the pin):

| Channel | GPIO pin | `FUNC_CFG` register   | Shift in register | Mode value |
|---------|----------|-----------------------|-------------------|------------|
| PWM0    | GPIO 6   | `FUNC_CFG_1` (+32×4)  | `6 × 2 = 12`      | `2`        |
| PWM1    | GPIO 7   | `FUNC_CFG_1`          | `7 × 2 = 14`      | `2`        |
| PWM2    | GPIO 8   | `FUNC_CFG_1`          | `8 × 2 = 16`      | `2`        |
| PWM3    | GPIO 9   | `FUNC_CFG_1`          | `9 × 2 = 18`      | `2`        |
| PWM4    | GPIO 24  | `FUNC_CFG_2` (+46×4)  | `(24-16) × 2 = 16`| `2`        |
| PWM5    | GPIO 26  | `FUNC_CFG_2` (+46×4)  | `(26-16) × 2 = 20`| `2`        |

Mode value `2` selects the pin's second peripheral function. Write it as a
2-bit field at the given shift.

**Step 2 — configure the per-pin GPIO register:**

| Bits  | Field                       | Purpose                                                        |
|-------|-----------------------------|-----------------------------------------------------------------|
| `[3]` | output enable               | 1 = enables the pin's output driver                             |
| `[6]` | peripheral function enable  | 1 = the muxed peripheral (not plain GPIO) drives the pin        |

Both bits must be set for the PWM channel to actually toggle the pin — output
enable alone leaves the pin under plain GPIO control.

> **Note on mode value variants:** vendor reference material documents two
> different `FUNC_CFG` mode-value mappings for GPIO pins 1–3 and 5. Mode value
> `2` is confirmed to select the PWM function on GPIO 26 (channel 5) on
> BK7221U silicon.

---

## Interrupt handling

All 6 channels share `IRQ_SOURCE_PWM` (bit 9 of ICU `irq_enable`). The ISR must
read `int_status`, determine which channels fired from bits `[5:0]`, then write
the same value back to clear them (write-1-to-clear). A channel running in PWM
output mode with `int_en = 0` never asserts the shared IRQ line — nothing needs
acknowledging for it.

---

## Hardware-confirmed (silicon)

| Fact | How verified |
|------|-------------|
| All 6 channel clocks enabled via ICU `peri_clk_pwd` bits [14:9] | Timer probe (separate block, same ICU field) |
| `duty` writable on-the-fly without disabling channel | LED breathing probe: duty updated 300 times while channel 5 ran |
| GPIO 26 → channel 5, mode value `2`, `FUNC_CFG_2` bits [21:20] | LED breathing probe: LED visibly varied in brightness for 3 full cycles |
| PWM output: pin high when `counter < duty`, low when `counter ≥ duty` | Consistent with breathing behavior (duty=0 → LED off, duty=period → LED on) |
