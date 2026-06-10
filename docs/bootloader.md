# Vendor Bootloader Reference (`bootloader_a9_v720.bin`)

Documents the vendor bootloader that ships in the `bootloader` flash partition
(`0x000000–0x00FFFF`). This bootloader is being replaced — see
`docs/superpowers/specs/2026-06-07-bootloader-design.md`.

---

## Exception vector dispatch

The bootloader's ARM exception vector table starts at `0x00000000` and
dispatches through a two-level indirection before reaching the hook table in RAM
(see `docs/memory_map.md` — *Hook table*).

### Level 1 — ARM vector table (`0x00000000–0x0000001C`)

```
0x00000000  EA0000C5  b <reset>           ; branches to reset handler at 0x000003CC
0x00000004  E59FF014  ldr pc, [pc, #0x14] ; UNDEFINED  → [0x00000020]
0x00000008  E59FF014  ldr pc, [pc, #0x14] ; SWI        → [0x00000024]
0x0000000C  E59FF014  ldr pc, [pc, #0x14] ; PABORT     → [0x00000028]
0x00000010  E59FF014  ldr pc, [pc, #0x14] ; DABORT     → [0x0000002C]
0x00000014  E59FF014  ldr pc, [pc, #0x14] ; RESERVED   → [0x00000030]
0x00000018  E59FF014  ldr pc, [pc, #0x14] ; IRQ        → [0x00000034]
0x0000001C  E59FF014  ldr pc, [pc, #0x14] ; FIQ        → [0x00000038]
```

### Level 2 — dispatch address table (`0x00000020–0x0000003C`)

| Address      | Value        | Points to                       |
|--------------|--------------|---------------------------------|
| `0x00000020` | `0x00000608` | Bootloader UNDEFINED handler    |
| `0x00000024` | `0x0000059C` | Bootloader SWI handler          |
| `0x00000028` | `0x00000618` | Bootloader PABORT handler       |
| `0x0000002C` | `0x00000628` | Bootloader DABORT handler       |
| `0x00000030` | `0x00000638` | Bootloader RESERVED handler     |
| `0x00000034` | `0x000005AC` | Bootloader IRQ handler          |
| `0x00000038` | `0x000005BC` | Bootloader FIQ handler          |
| `0x0000003C` | `0xDEADBEEF` | Sentinel / padding              |

### Level 3 — bootloader handlers → hook table dispatch

Each handler body follows the same pattern (verified by disassembly):

```asm
push  {r0, r1}
ldr   r1, =<hook_slot_address>   ; from literal pool
ldr   r1, [r1]                   ; read firmware handler from hook table
bx    r1                          ; tail-call dispatch
```

Hook slot addresses read by each handler:

| Handler   | Hook slot                                                                     |
|-----------|-------------------------------------------------------------------------------|
| IRQ       | `0x00400000` (`hook_IRQ`) — loaded via `mov r1, #0x400000` (immediate)       |
| FIQ       | `0x00400004` (`hook_FIQ`)                                                     |
| UNDEFINED | `0x0040000C` (`hook_undefined`) — confirmed routed to firmware handler        |
| PABORT    | `0x00400010` (`hook_pabort`)                                                  |
| DABORT    | `0x00400014` (`hook_dabort`) — reads correct slot; routing to IRAM handler unconfirmed |
| RESERVED  | `0x00400018` (`hook_reserved`)                                                |

---

## Boot sequence

The reset handler at `0x000003CC` performs ARM startup (CPSR mode switches, stack
painting for all modes, ZI init), then calls the main init function
`FUN_000015c4`.  The call chain relevant to clock bringup is:

```
_reset (0x000003CC)
  └─ FUN_000015c4          (top-level init)
       └─ FUN_000007c4     (power / clock init)
            ├─ FUN_00001654    (LPO clock source + ROSC calibration trigger)
            └─ FUN_000015e8   (DPLL VCO analog programming)
```

After `FUN_000007c4` returns, `FUN_000015c4` switches `mclk` to DPLL/4
(~120 MHz), sets the ICU peripheral clock mux, and trims the LDO bias before
continuing with GPIO and UART init.

The sequence was reverse-engineered from `bootloaders/bootloader_a9_v720.bin`
(Ghidra disassembly) and verified by flashing a minimal proof-of-concept that
produced UART2 output.
The description below maps each raw register write to the named fields in
`hardware/sctrl_regs.h` and `hardware/icu.h`.

---

## PLL initialization

The CPU starts from reset on the DCO clock.  Without running this sequence
UART output is unreliable.  All 10 steps must execute in order; there is no PLL
lock status bit — the vendor uses fixed delays only.

### Delay helpers

Two delay primitives are used throughout the sequence.  Both are busy-wait
loops with no calibration — timing is approximate and depends on CPU clock.

```c
/* raw_delay: counts down n iterations (~2 cycles each at -O2).
   At DCO boot clock (~26 MHz): raw_delay(0x514) ≈ 100 µs. */
static void raw_delay(uint32_t n) {
    __asm__ volatile("1: subs %0, #1\n bcs 1b\n" : "+r"(n) : : "cc");
}

/* coarse_delay: raw_delay(outer * 28).
   At DCO boot clock: coarse_delay(100) ≈ 215 µs. */
static void coarse_delay(uint32_t outer) {
    uint32_t n = outer * 0x1cu;
    __asm__ volatile("1: subs %0, #1\n bcs 1b\n" : "+r"(n) : : "cc");
}
```

Steps 8–10 execute after the `mclk` switch to DPLL/4 (~120 MHz), so
`coarse_delay(100)` there is ~4× shorter in wall time than during steps 1–7.

### Step 1 — enable core analog blocks (`hw_sctrl->block_enable`)

```c
hw_sctrl->block_enable.v =
    (hw_sctrl->block_enable.v & 0x000fffffu) | 0xA5C0043Fu;
```

Sets `write_key = 0xA5C` (required on every write) and enables:
flash, dco, rosc_32k, xtal_26m, xtal_32k, dpll_480m, xtal_2_rf.
Other `block_enable` bits are preserved.

### Step 2 — release subsystem power domains

```c
hw_sctrl->power_mac_modem.v = 0;
hw_sctrl->power_dsp.v       = 0;
hw_sctrl->power_usb.v       = 0;
// coarse_delay(100)
```

Zeroing these registers prevents RF/modem activity from disturbing the analog
clock during init.

### Step 3 — LPO clock source (`hw_sctrl->low_power_clk`)

```c
hw_sctrl->low_power_clk.v = 0;   // lpo_clk_mux = 0 → ROSC
```

### Step 4 — trigger ROSC one-shot calibration (`hw_sctrl->rosc_cal`)

```c
hw_sctrl->rosc_cal.v = 0x37;
// cal_en=1, cal_trig=1, cal_mode=1 (one-shot), cal_interval=3
```

### Step 5 — re-arm `block_enable` write_key before VCO programming

```c
hw_sctrl->block_enable.v =
    (hw_sctrl->block_enable.v & 0x000fffffu) | 0xA5C00008u;
```

Issues a fresh `write_key` pulse without changing the data bits set in step 1.
Required by the analog SPI state machine before programming VCO registers.

### Step 6 — program DPLL VCO analog registers

```c
hw_sctrl->analog_ctrl0 = 0xF819A59Bu;
hw_sctrl->analog_ctrl1 = 0x6AC03102u;
hw_sctrl->analog_ctrl2 = 0x24026040u;
hw_sctrl->analog_ctrl3 = 0x4FE06C50u;
hw_sctrl->analog_ctrl4 = 0x59C04520u;
```

`analog_ctrl0–4` are not field-decoded in `sctrl_regs.h`.  Known details:

- `analog_ctrl0` bit 31 — XTAL drive current.  The BK7252 vendor bootloader
  sets it (`0xF819A59B`); the BK7231U SDK clears it (`0x0819A59B`).  The
  difference is chip-specific; do not clear bit 31 on BK7252.
- `analog_ctrl0` contains two DPLL calibration control bits (`SPI_TRIG_BIT`,
  `SPI_DET_EN`; exact positions not decoded).  Both are set in the initial
  value above.  After boot these same bits are used by the DPLL recalibration
  sequence — see *Post-init DPLL recalibration* below.
- `analog_ctrl1`, `analog_ctrl3` — identical between BK7252 and BK7231U.
- `analog_ctrl4 = 0x59C04520` — matches BK7231U (BK7231 uses `0x59E04520`).
- Do not alter any of these values without a working oscilloscope on the VCO
  output.

### Step 7 — wait for DPLL to lock

```c
// raw_delay(0x514)
// coarse_delay(100)
```

Fixed delays only.  No lock-status bit exists (or is not used by the vendor).

### Step 8 — switch `mclk` to DPLL (`hw_sctrl->control`)

```c
hw_sctrl->control.v =
    (hw_sctrl->control.v & ~0xf3u) | 0x32u;
// mclk_source = 2 (DPLL), divider = 3 → CPU clock = DPLL / 4 ≈ 120 MHz
// coarse_delay(100)
```

`control.mclk_source = 2` (DPLL) and `control.divider = 3` (divide-by-4).
DPLL runs at ~480 MHz, so CPU clock becomes ~120 MHz.

`control.dpll_div_reset` (bit 14) is relevant for sleep/wakeup: assert it
before shutting DPLL down, release it after DPLL blocks are re-enabled and
`mclk` has been temporarily switched to 26M XTAL, then switch back to DPLL.

### Step 9 — set peripheral clock mux (`hw_icu->peri_clk_mux`)

```c
hw_icu->peri_clk_mux.v = 0x83;
// uart1=PERI_CLK_26M, uart2=PERI_CLK_26M, pwms=PERI_CLK_26M
// coarse_delay(100)
```

UART1, UART2, and the PWM timers are clocked from the 26 MHz XTAL regardless
of the CPU clock source.  This means the UART baud divider (0xe1 = 225) is
valid before and after the `mclk` switch in step 8:
`26 000 000 / 225 ≈ 115 555 baud ≈ 115200`.

### Step 10 — manual LDO bias calibration (`hw_sctrl->bias`)

```c
hw_sctrl->bias.v =
    (hw_sctrl->bias.v & 0xFFFFE0EFu) | 0x1410u;
// cal_manual = 1, ldo_val_manual = 20 (0x14)
```

Overrides the auto-calibrated LDO value with the vendor-determined constant 20.
The BK7231U SDK uses `ldo_val_manual = 23` (`0x17`) for the same field; the
BK7252 vendor bootloader uses 20.  This is the last step before the clock
system is considered stable.

---

### Post-init DPLL recalibration

After boot the DPLL can lose lock (e.g. due to temperature or RF interference).
The chip fires a FIQ (`FIQ_DPLL_UNLOCK`) when this happens.  The handler must
re-run the calibration SPI sequence on `analog_ctrl0`:

```c
uint32_t val = hw_sctrl->analog_ctrl0;

// clear SPI_TRIG_BIT, then set it — triggers the analog SPI transaction
val &= ~SPI_TRIG_BIT;
hw_sctrl->analog_ctrl0 = val;
raw_delay(60);                   // ~10 µs at DCO boot clock
val |= SPI_TRIG_BIT;
hw_sctrl->analog_ctrl0 = val;

// clear then re-set SPI_DET_EN — re-arms the unlock detector
val = hw_sctrl->analog_ctrl0;
val &= ~SPI_DET_EN;
hw_sctrl->analog_ctrl0 = val;
raw_delay(340);                  // ~200 µs at DCO boot clock; use coarse_delay(1) after mclk switch
val |= SPI_DET_EN;
hw_sctrl->analog_ctrl0 = val;
```

`SPI_TRIG_BIT` and `SPI_DET_EN` are bit positions within `analog_ctrl0` that
are not yet decoded in `sctrl_regs.h`.  The same sequence is required after
wakeup from hardware sleep before switching `mclk` back to DPLL.
