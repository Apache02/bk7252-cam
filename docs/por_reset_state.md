# BK7252 Power-On Reset State

Measured register values captured immediately at bootloader entry, before any
driver initialisation (`sctrl_init`, `uart2_init`, etc.).  Capture method: raw
`uint32_t` reads into a `.noinit` buffer as the first action of `preinit()` in
`src/applications/bootloader/main.cpp`.

All values reflect a cold power-on reset.  Where a WDT-reboot difference exists
it is noted explicitly.

---

## SCTRL (`0x00800000`)

### Chip identity

| Word | Offset | Value        | Meaning                              |
|------|--------|--------------|--------------------------------------|
| [00] | +0x000 | `0x0007221a` | `chip_id` — BK7221U confirmed        |
| [01] | +0x004 | `0x18221020` | `device_id` — silicon revision       |

### Clock configuration

| Word | Offset | Value        | Field decode                                                        |
|------|--------|--------------|---------------------------------------------------------------------|
| [02] | +0x008 | `0x00020000` | `mclk_source=0` → **DCO** (not DPLL, not XTAL); `qspi_io_volt=2`  |
| [07] | +0x01c | `0x00000207` | `flash_delay=0x207` (519 MCU cycles at DCO rate)                    |

The chip boots from the internal DCO oscillator.  DPLL and the 26 MHz crystal
are **not enabled at POR** — they are off until firmware explicitly enables them
via `pll_init` / `sctrl_init`.

`peri_clk_pwd` shows all 24 peripheral clock domains powered down at POR,
including UART2.  UART2 requires clock initialisation before use; it does not
work on raw DCO without the PLL init sequence.

### Enabled analog blocks at POR

`block_enable` (+0x12c) = `0x00000147`:

| Bit | Block              | State at POR |
|-----|--------------------|--------------|
| 0   | flash              | **on**       |
| 1   | DCO                | **on**       |
| 2   | ROSC 32 kHz        | **on**       |
| 3   | XTAL 26 MHz        | off          |
| 4   | XTAL 32 kHz        | off          |
| 5   | DPLL 480 MHz       | off          |
| 6   | digital core       | **on**       |
| 8   | analog sys LDO     | **on**       |

### Power domains

`pmu_status` (+0x118) = `0x0000001f`: USB, DSP, modem, and MAC subsystems are
all **powered down** at POR.  Bit 4 is also set (beyond the documented 4 bits);
identity not confirmed, presumed a fifth power domain.

`clk_gating` (+0x00c) = `0x000001ff`: all nine MAC-related clock gating bits
set — MAC subsystem clocks are gated off.

`reset` (+0x010) = `0x00000028`: `tl410_boot=1`, `tl410_ext_wait=1`.

### Sleep configuration (pre-loaded by vendor ROM)

`sleep` (+0x104) = `0x00a60000`:

- `flash_pwd_sleep=1`, `dco_pwd_sleep=1` — flash and DCO power down in sleep
- `procore_dly=10` — 10-cycle delay before CPU resumes after power-core-on

`digital_vdd` (+0x108) = `0x00000040`: `vdd_active=4` → **digital VDD = 1.12 V**
during active operation.

### Analog controls (DPLL / DCO / LDO)

Raw values at POR; non-zero means the vendor ROM has pre-loaded calibration:

| Word | Offset | Value        |
|------|--------|--------------|
| [22] | +0x058 | `0x61105b57` |
| [23] | +0x05c | `0x68c03102` |
| [24] | +0x060 | `0x84036080` |
| [25] | +0x064 | `0x180004a0` |
| [26] | +0x068 | `0x84200a52` |
| [27] | +0x06c | `0x3b13b13b` |
| [28] | +0x070 | `0x00b09350` |

`analog_ctrl6` (+0x070) decoded: `dpll_reset=0`, `clk_for_usb_en=0`,
`clk_for_audio_en=0` — DPLL output is not routed anywhere at POR.

`analog_spi` (+0x040) = `0x3c000000`: state=0 (idle), `freq_div=60`.

### ROSC calibration

`rosc_cal` (+0x134) = `0x00000030`: `cal_en=0` (calibration not running),
`cal_interval=3`.

### Reset type detection

`sw_retention` (+0x150) = `0x00000000` at POR.  This register survives
sleep-wake and soft reset (jump to 0x0) but is cleared by a true power-on or
WDT reset.  Firmware can write a sentinel here to distinguish warm reboot from
cold POR on the next entry.

---

## ICU (`0x00802000`)

### Peripheral clock state at POR

`peri_clk_mux` (+0x000) = `0x00000000`: all peripherals clocked from DCO.

`peri_clk_pwd` (+0x008) = `0x00ffffff`: all 24 peripheral clock domains are
**powered down** (UART1, UART2, I2C1, IRDA, I2S, I2C2, SPI, SAR ADC, WDT,
PWM0–5, Audio, TL410 WDT, SDIO, USB, FFT, Timer, JPEG, QSPI).

`peri_clk_gate_disable` (+0x00c) = `0x0001ffff`: all clock gates disabled
(clocks forced-on if the corresponding pwd bit is cleared).

`tl410_clk_pwd` (+0x010) = `0x00000003`: TL410 and BLE clocked off.

`irq_enable` (+0x040) = `0x00000000`: no interrupts enabled.

---

## Flash controller (`0x00803000`)

| Word | Offset | Value        | Meaning                                                   |
|------|--------|--------------|-----------------------------------------------------------|
| [05] | +0x014 | `0x28390000` | `m_value=0xa0` (quad M-byte, set by vendor ROM); `wr_fifo_ptr=7` (constant) |
| [07] | +0x01c | `0x0400000f` | `clk_conf=15`; `model_sel=0` (single SPI); **`crc_en=1`** (XIP CRC active) |
| [08] | +0x020 | `0x01ffffff` | "reserved" on BK7221U — non-zero; identity unknown        |

CRC-verified XIP mode is active from the moment the vendor bootloader hands
control over.  `clk_conf=15` maps to an undocumented flash SPI clock divider
(likely a slow DCO-derived rate used before DPLL comes up).

---

## GPIO (`0x00802800`)

### Bank 0 default (GPIO 0–19, 24–31)

All read `0x00000028`: `output_enable=1`, `pull_enable=1`, `pull_mode=0`
(**pull-down** with output driver enabled).

### GPIO 20–23

Read `0x00000068`: same as above plus **`function_enable=1`** (alternate
function active).  `FUNC_CFG_2` (+0x0b8) = `0x00005500` confirms
`PERIAL_MODE_2` for GPIO 20–23.  These four pins are routed to a peripheral
(likely UART or SPI) by the vendor ROM before bootloader entry.

### GPIO function config registers (offsets +0x080–+0x0bc)

| Array index | Offset | Value        | Note                                              |
|-------------|--------|--------------|---------------------------------------------------|
| [39]        | +0x09c | `0x00000002` | identity unknown; single bit set                  |
| [46]        | +0x0b8 | `0x00005500` | `FUNC_CFG_2`: GPIO 20–23 in `PERIAL_MODE_2`       |
| [47]        | +0x0bc | `0x00000000` | `FUNC_CFG_3`: GPIO 32–39 all in `PERIAL_MODE_1`   |

### Bank 1 (GPIO 32–39)

All read `0x00000028`: same pull-down default as bank 0.

---

## WDT (`0x00802900`)

`ctrl` = `0x00004000`: `period=0x4000` (16384 cycles).  The vendor bootloader
configures the watchdog before handing control; the period value at entry
reflects the vendor ROM's WDT setting.

---

## POR vs WDT reboot: the one difference

`SCTRL[33]` `charge_status` (+0x084):

| Reset type | Value        | `vcal` (bits [10:5]) |
|------------|--------------|----------------------|
| Cold POR   | `0xffe08410` | 32 (bit 10 set)      |
| WDT reboot | `0xffe08010` | 0  (bit 10 clear)    |

The `charge_status` fields are documented as read-only charger calibration
results.  Analog blocks (LDO, charger) are **not power-cycled by a WDT reset**,
so the calibration state they hold at POR differs from what persists after a
soft reset.

**Reliable reset-type detection:**

1. `sw_retention == 0` → POR or WDT reset (register is cleared by both).
   Write a sentinel here during init to detect warm reboot on next entry.
2. `charge_status` bit 10 set → cold POR (analog calibration ran fully).
   Cleared after WDT/soft reset.
3. `pmu_status` bit values can help if subsystem power state was changed before
   the reset.
