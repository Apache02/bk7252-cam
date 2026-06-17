# Flash SPI Bypass — Investigation Notes

This document records the state of the `hardware_flash` bypass driver as of June 2026.
The driver builds and links but `flash_uid()` returns all-zero bytes.
Use this as a starting point when resuming work.

## What the driver is supposed to do

`flash_bypass_xfer` switches the flash SPI bus from the dedicated flash controller
to the user SPI peripheral (SPI0 @ `0x00802700`), sends a command, receives a
response, then restores the flash controller.  This is needed for any flash command
that the dedicated controller does not implement — specifically `4Bh Read Unique ID`.

`flash_uid(uid[16])` calls `flash_bypass_xfer` with command `{0x4B, 0x00, 0x00, 0x00, 0x00}`
and reads 16 response bytes.

## What has been confirmed working

- The driver builds without warnings or errors.
- `flash_bypass_xfer` runs without crashing (no prefetch abort, no WDT fire).
- The SPI peripheral clocks out bytes — `rxfifo_empty` does go low after each TX write,
  meaning the SPI shift register is actually running.
- `flash_spi_mux = 1` can be set and verified (the loop terminates).
- NSSMD=2 (CS# assert) / NSSMD=3 (CS# deassert) is the correct CS# mechanism for this
  SPI peripheral.  This is confirmed by the SDK's `flash_bypass_wr_sr_cb` constants:
  `SPI_VAL_TAKE_CS = 0x02`, `SPI_VAL_RELEASE_CS = 0x03`.
- ICU SPI clock setup: `peri_clk_mux.spi = PERI_CLK_26M_XTAL`, `peri_clk_pwd.spi = 0`.
- GPIO14-17 must be set to Hi-Z (0x08) before `flash_spi_mux = 1` to prevent output
  driver contention.  The SDK's `flash_bypass_wr_sr_cb` (BK7221U branch) does this
  explicitly (`GMODE_SET_HIGH_IMPENDANCE = 0x08`).

## Symptom

```
[ OK ] flash_uid returns true
[FAIL] UID has at least one non-zero byte
uid: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

The SPI completes without hanging.  Every received byte is `0x00`.

## Root cause hypothesis (most likely, untested as of this commit)

The GPIO function routing registers are not configured before `flash_spi_mux = 1`.

The GPIO peripheral has two additional register banks that control which peripheral
function drives each physical pin — distinct from the per-pin config register
(`GPIO_BASE + gpio_num*4`):

| Register | Address | Purpose |
|---|---|---|
| `REG_GPIO_FUNC_CFG`    | `0x00802880` (GPIO_BASE + 32×4) | 2-bit function select per GPIO, GPIO0–15 |
| `REG_GPIO_FUNC_CFG_2`  | `0x008028B8` (GPIO_BASE + 46×4) | 2-bit function select per GPIO, GPIO16–31 |
| `REG_GPIO_MODULE_SELECT` | `0x008028B4` (GPIO_BASE + 45×4) | Which peripheral module owns the SPI pins |

For GPIO14-17 to carry SPI signals, these registers must be set to `PERIAL_MODE_1 = 1`:

```
REG_GPIO_FUNC_CFG    bits[29:28] = 01  (GPIO14 → SPI)
REG_GPIO_FUNC_CFG    bits[31:30] = 01  (GPIO15 → SPI)
REG_GPIO_FUNC_CFG_2  bits[1:0]   = 01  (GPIO16 → SPI)
REG_GPIO_FUNC_CFG_2  bits[3:2]   = 01  (GPIO17 → SPI)
REG_GPIO_MODULE_SELECT bits[1:0] = 01  (GPIO_SPI_MODULE)
```

The SDK's `spi_gpio_configuration()` (called from `spi_init_msten()`) does exactly this
via `CMD_GPIO_ENABLE_SECOND(GFUNC_MODE_SPI)`.  Our driver never calls this, so the SPI
peripheral's MISO input is not connected to GPIO14 — the SPI reads 0 for every bit.

The SDK's write-only bypass (`flash_bypass_wr_sr_cb`) works despite the same issue
because it calls `bk_spi_configure()` → `spi_init_msten()` → `spi_gpio_configuration()`
before the bypass window.  Our driver skips this step.

Why this was not caught earlier: the bootloader may leave these registers set from a
previous SPI operation, making the driver appear to work on the first run after a
cold boot and fail after a warm reset.  This was not systematically tested.

## Proposed fix

Before setting `flash_spi_mux = 1`, configure the GPIO function routing for GPIO14-17:

```c
// Save and configure GPIO function routing for SPI.
// REG_GPIO_FUNC_CFG   = GPIO_BASE + 32*4 = 0x00802880
// REG_GPIO_FUNC_CFG_2 = GPIO_BASE + 46*4 = 0x008028B8
// REG_GPIO_MODULE_SELECT = GPIO_BASE + 45*4 = 0x008028B4
// PERIAL_MODE_1 = 1; GPIO_SPI_MODULE = 1

#define REG_GPIO_FUNC_CFG_BP    (*(volatile uint32_t *)0x00802880u)
#define REG_GPIO_FUNC_CFG_2_BP  (*(volatile uint32_t *)0x008028B8u)
#define REG_GPIO_MODULE_SEL_BP  (*(volatile uint32_t *)0x008028B4u)

uint32_t func_cfg_saved  = REG_GPIO_FUNC_CFG_BP;
uint32_t func_cfg2_saved = REG_GPIO_FUNC_CFG_2_BP;
uint32_t mod_sel_saved   = REG_GPIO_MODULE_SEL_BP;

// GPIO14 bits[29:28]=01, GPIO15 bits[31:30]=01
REG_GPIO_FUNC_CFG_BP  = (func_cfg_saved  & ~0xF0000000u) | 0x50000000u;
// GPIO16 bits[1:0]=01, GPIO17 bits[3:2]=01
REG_GPIO_FUNC_CFG_2_BP = (func_cfg2_saved & ~0x0000000Fu) | 0x00000005u;
// SPI module select bit[0]=1
REG_GPIO_MODULE_SEL_BP = (mod_sel_saved   & ~0x00000003u) | 0x00000001u;
```

Restore all three at the end of the transfer (after `flash_spi_mux = 0`).

After this fix, also consider adding a JEDEC ID check (`0x9F`, 3 bytes, no dummy) as a
sanity probe before the UID command — JEDEC ID should return `0xEF/0xXX/0xXX`
(manufacturer byte non-zero) and is a quick way to confirm SPI read-back works.

## What does NOT work on BK7221U (from SDK)

The SDK's `flash_bypass_op_read` (full SPI read bypass) exists **only** for
`SOC_BK7238`, `SOC_BK7252N`, and `SOC_BK7231N`.  The BK7221U (= BK7252 as used
in this project) has no read bypass in the SDK.  Our driver is a novel
implementation — there is no SDK reference implementation for it on this chip.

## Files

```
src/platform/drivers/hardware_flash/flash_bypass.c          driver implementation
src/platform/drivers/hardware_flash/flash_bypass_regs.h     SPI/GPIO/ICU register layout
src/platform/drivers/hardware_flash/include/hardware/flash_bypass.h  public API
src/tests/probe/probe.cpp                                   on-chip test (flash_uid)
src/tests/probe/CMakeLists.txt                              links hardware_flash
```
