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

Set the channel's bit to `0` (0 = clock running, 1 = powered down).
PWM channels 0–5 occupy bits [14:9].

```c
hw_icu->peri_clk_pwd.pwm5 = 0;  // enable channel 5 clock
```

### 2. Select clock source — `pwm_clk_mux`

One bit per channel; bits [5:0] correspond to channels 0–5.

| Value | Source         |
|-------|----------------|
| `0`   | PCLK (~26 MHz) |
| `1`   | LPO (32 kHz)   |

```c
hw_icu->pwm_clk_mux.pwm5 = 0;  // PCLK
```

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
to one fixed GPIO pin. Two steps are required to connect a channel to its pin:

**Step 1 — write `FUNC_CFG` register** (selects which peripheral drives the pin):

| Channel | GPIO pin | `FUNC_CFG` register   | Shift in register | Mode value      |
|---------|----------|-----------------------|-------------------|-----------------|
| PWM0    | GPIO 6   | `FUNC_CFG_1` (+32×4)  | `6 × 2 = 12`      | `PERIAL_MODE_2` |
| PWM1    | GPIO 7   | `FUNC_CFG_1`          | `7 × 2 = 14`      | `PERIAL_MODE_2` |
| PWM2    | GPIO 8   | `FUNC_CFG_1`          | `8 × 2 = 16`      | `PERIAL_MODE_2` |
| PWM3    | GPIO 9   | `FUNC_CFG_1`          | `9 × 2 = 18`      | `PERIAL_MODE_2` |
| PWM4    | GPIO 24  | `FUNC_CFG_2` (+46×4)  | `(24-16) × 2 = 16`| `PERIAL_MODE_2` |
| PWM5    | GPIO 26  | `FUNC_CFG_2` (+46×4)  | `(26-16) × 2 = 20`| `PERIAL_MODE_2` |

`PERIAL_MODE_2 = 1` (from `soc/gpio.h`). Write as a 2-bit field at the given shift.

**Step 2 — configure the per-pin register** (`hw_gpio_bank0[pin]`):

```c
hw_gpio_bank0[pin].v = GPIO_OUTPUT_ENABLE_BIT | GPIO_FUNCTION_ENABLE_BIT;
// GPIO_OUTPUT_ENABLE_BIT  = (1 << 3)  — enables the output driver
// GPIO_FUNCTION_ENABLE_BIT = (1 << 6) — peripheral controls the pin
```

Full example for GPIO 26 / channel 5:

```c
// ICU
hw_icu->peri_clk_pwd.pwm5 = 0;
hw_icu->pwm_clk_mux.pwm5  = 0;  // PCLK

// GPIO mux
volatile uint32_t *cfg2 = (volatile uint32_t *)REG_GPIO_FUNC_CFG_2;
*cfg2 = (*cfg2 & ~(0x3u << 20)) | ((uint32_t)PERIAL_MODE_2 << 20);
hw_gpio_bank0[26].v = GPIO_OUTPUT_ENABLE_BIT | GPIO_FUNCTION_ENABLE_BIT;

// PWM channel 5 — period=26000 (1 kHz), duty=13000 (50%)
hw_pwm->ch[5].period = 26000;
hw_pwm->ch[5].duty   = 13000;
// enable ch5: en=1, int_en=0, mode=0 at bits [23:20]
hw_pwm->ctl.v = (hw_pwm->ctl.v & ~(0xFu << 20)) | (1u << 20);
```

> **Note on GPIO pin variants:** The SDK `gpio.c` contains two functions with
> different `PERIAL_MODE` values for channels 1–3 and 5 between them. `PERIAL_MODE_2`
> is from the second function and is confirmed to work on BK7221U silicon (GPIO 26 /
> channel 5 verified with the LED breathing probe).

---

## Interrupt handling

All 6 channels share `IRQ_SOURCE_PWM` (bit 9 of ICU `irq_enable`). The ISR must
read `hw_pwm->int_status.v`, determine which channels fired, then write the same
value back to clear them (W1C). Do not call `intc_enable_irq_source` for channels
running in PWM output mode (`int_en = 0`) — they do not assert the IRQ.

```c
void pwm_isr(void) {
    uint32_t fired = hw_pwm->int_status.v;
    hw_pwm->int_status.v = fired;          // W1C
    for (unsigned i = 0; i < 6; i++) {
        if (fired & (1u << i)) { /* handle channel i */ }
    }
}
```

---

## Using `hardware_pwm`

The `hardware_pwm` driver wraps the above into a small API. It manages the ICU
clock gate automatically. **The caller is responsible for GPIO pin setup** (both
`FUNC_CFG` and per-pin register) before calling `pwm_output_init`.

```c
#include "hardware/pwm.h"

// PWM output — 1 kHz, 25% duty on whatever pin channel 2 maps to
pwm_output_init(2, 26000, 6500);
pwm_output_set_duty(2, 19500);  // raise to 75% on the fly
pwm_output_deinit(2);

// Periodic timer — fires callback at 1 kHz
void tick(void) { /* IRQ context */ }
pwm_timer_init(3, 26000, tick);
pwm_timer_deinit(3);
```

---

## Hardware-confirmed (silicon)

| Fact | How verified |
|------|-------------|
| All 6 channel clocks enabled via ICU `peri_clk_pwd` bits [14:9] | Timer probe (separate block, same ICU field) |
| `duty` writable on-the-fly without disabling channel | LED breathing probe: duty updated 300 times while channel 5 ran |
| GPIO 26 → channel 5, `PERIAL_MODE_2`, `FUNC_CFG_2` bits [21:20] | LED breathing probe: LED visibly varied in brightness for 3 full cycles |
| PWM output: pin high when `counter < duty`, low when `counter ≥ duty` | Consistent with breathing behavior (duty=0 → LED off, duty=period → LED on) |
