# BK7252 Memory Map

ARM968E-S, 32-bit address space. All addresses are byte addresses; the bus is
little-endian. Flash appears twice (XIP window + mirror); RAM appears in two
physically separate blocks.

---

## Address Space Summary

| Range                       | Size   | Region                              |
|-----------------------------|--------|-------------------------------------|
| `0x00000000`–`0x0000FFFF`   | 64 KiB | Flash XIP — Bootloader / ROM        |
| `0x00010000`–`0x0012FFFF`   | ~1.1 MiB | Flash XIP — Application           |
| `0x00130000`–`0x001FFFFF`   | ~832 KiB | Flash XIP — OTA / reserved         |
| `0x00200000`–`0x003FFFFF`   | 2 MiB  | Flash upper window: 4 MiB chip → upper 2 MiB of flash; 2 MiB chip → mirror of `0x0–0x1FFFFF` |
| `0x00400000`–`0x0043FFFF`   | 256 KiB | RAM Block 1 (main firmware RAM)    |
| `0x00440000`–`0x007FFFFF`   | —      | RAM Block 1 mirrors                 |
| `0x00800000`–`0x008FFFFF`   | 1 MiB  | Peripheral bus                      |
| `0x00900000`–`0x0093FFFF`   | 256 KiB | RAM Block 2 (IRAM / coprocessor)   |
| `0x01000000`                | —      | MDM / AGC (modem + baseband AGC)    |
| `0x01050000`                | —      | RC (RF transceiver control)         |
| `0x01060000`                | —      | MPB (MAC PHY Bypass)                |
| `0x10910000`                | —      | INTC (NX coprocessor interrupt controller) |
| `0x10E00000`                | —      | LA (logic analyzer)                 |
| `0xC0000000`–`0xC001006C`   | —      | NXMAC (MAC core + PL + PTA)         |

---

## Flash (XIP)

Physical capacity: 2 MiB or 4 MiB depending on the installed chip variant (one built-in SPI NOR flash).

### CRC interleaving

The flash controller operates in a CRC-aware XIP mode for the application
region. Every 32 bytes of logical data is stored as 34 physical bytes: 32 data
bytes followed by a 2-byte CRC16-CCITT (polynomial `0x8005`, initial value
`0xFFFF`, big-endian). The controller strips the CRC bytes on read.

Mapping from logical byte offset `n` to physical flash byte offset:

```
physical = (n / 32) * 34 + (n % 32)
```

Consequence: logical `0x00010000` (linker `ORIGIN`) maps to physical flash byte
`0x00011000` (programmed by the flasher at `--startaddr 0x11000`).

### Flash segment layout

| Logical range               | Physical flash  | Content                                  |
|-----------------------------|-----------------|------------------------------------------|
| `0x00000000`–`0x0000FFFF`   | `0x000000`      | Bootloader, ARM exception vectors, RF/MAC firmware |
| `0x00010000`–`0x0012FFFF`   | `0x011000`      | Application (CRC-interleaved)            |
| `0x00130000`–`0x001FFFFF`   | —               | OTA / reserved (2 MiB chip end)          |
| `0x00200000`–`0x003FFFFF`   | —               | Upper 2 MiB (4 MiB chip only)            |

Flasher segment names (`uartprogram --segment <name>`):

| Name         | Physical start  | Physical length | Notes                              |
|--------------|-----------------|-----------------|------------------------------------|
| `bootloader` | `0x000000`      | `0x010000`      | Write-protected after factory programming |
| `app`        | `0x011000`      | `0x119000`      | CRC-wrapped `app_crc.bin`          |

### Bootloader exception vectors (`bootloader_a9_v720.bin`)

The bootloader occupies `0x00000000`–`0x0000FFFF`. Its ARM exception vector
table starts at `0x00000000` and dispatches through a two-level indirection:

**Level 1 — ARM vector table (`0x00000000`–`0x0000001C`)**

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

**Level 2 — dispatch address table (`0x00000020`–`0x0000003C`)**

| Address      | Value        | Points to                                      |
|--------------|--------------|------------------------------------------------|
| `0x00000020` | `0x00000608` | Bootloader UNDEFINED handler                   |
| `0x00000024` | `0x0000059C` | Bootloader SWI handler                         |
| `0x00000028` | `0x00000618` | Bootloader PABORT handler                      |
| `0x0000002C` | `0x00000628` | Bootloader DABORT handler                      |
| `0x00000030` | `0x00000638` | Bootloader RESERVED handler                    |
| `0x00000034` | `0x000005AC` | Bootloader IRQ handler                         |
| `0x00000038` | `0x000005BC` | Bootloader FIQ handler                         |
| `0x0000003C` | `0xDEADBEEF` | Sentinel / padding                             |

**Level 3 — bootloader handlers → hook table dispatch**

Each handler body follows the same pattern (verified by disassembly):

```asm
push  {r0, r1}
ldr   r1, =<hook_slot_address>   ; from literal pool
ldr   r1, [r1]                   ; read firmware handler from hook table
bx    r1                          ; tail-call dispatch
```

The hook slot addresses used:

| Handler | Hook slot read |
|---|---|
| IRQ | `0x00400000` (`hook_IRQ`) — loaded via `mov r1, #0x400000` (immediate) |
| FIQ | `0x00400004` (`hook_FIQ`) |
| UNDEFINED | `0x0040000C` (`hook_undefined`) — **confirmed routed to firmware handler** |
| PABORT | `0x00400010` (`hook_pabort`) |
| DABORT | `0x00400014` (`hook_dabort`) — reads correct hook slot; routing to IRAM handler unconfirmed |
| RESERVED | `0x00400018` (`hook_reserved`) |

### Upper flash window (`0x00200000`–`0x003FFFFF`)

Behavior depends on the installed flash chip variant:

- **2 MiB chip** — the XIP controller aliases the entire 2 MiB flash into both
  halves of the 4 MiB window. Reads from `0x00200000`–`0x003FFFFF` return the
  same raw physical bytes as `0x00000000`–`0x001FFFFF` (without CRC decoding).
- **4 MiB chip** — `0x00200000`–`0x003FFFFF` maps to the upper 2 MiB of the
  physical flash (physical bytes `0x200000`–`0x3FFFFF`), not a mirror. These
  bytes are accessible raw via the OPERATE_SW path and as CRC-decoded XIP data
  via the XIP path, the same as the lower window.

Address bits above the flash capacity are insignificant (standard SPI flash
addressing), so `0x400000` is used as a base for all dump segments — it aliases
physical `0x000000` on both chip variants. On a 4 MiB chip consecutive sectors
at `0x400000, 0x401000, …, 0x7FF000` correctly wrap through the full 22-bit
physical space, covering both halves.

`uartreader --segment full2m` dumps 2 MiB from physical `0x0`.
`uartreader --segment full4m` dumps 4 MiB from physical `0x0` (for 4 MiB chips).

---

## RAM Block 1

Base: `0x00400000`.  Size: 256 KiB (`0x00400000`–`0x0043FFFF`).

The block is mirrored at `0x00440000`–`0x007FFFFF` (7 additional 256 KiB
aliases). Firmware uses the canonical `0x004xxxxx` range; mirrors are not used.

### Hook table

`0x00400000`–`0x0040001F` (32 bytes). Written by the bootloader before handing
control to the application. The application re-writes these slots at init time
(`boot_init.c`) to redirect exceptions to its own handlers.

| Address        | Symbol                   | Exception            |
|----------------|--------------------------|----------------------|
| `0x00400000`   | `hook_IRQ`               | IRQ                  |
| `0x00400004`   | `hook_FIQ`               | FIQ                  |
| `0x00400008`   | `hook_SWI`               | Software interrupt   |
| `0x0040000C`   | `hook_undefined`         | Undefined instruction |
| `0x00400010`   | `hook_pabort`            | Prefetch abort       |
| `0x00400014`   | `hook_dabort`            | Data abort           |
| `0x00400018`   | `hook_reserved`          | Reserved             |
| `0x0040001C`   | —                        | Unused / padding     |

### Firmware RAM layout (flash build)

| Address range               | Section              | Notes                                 |
|-----------------------------|----------------------|---------------------------------------|
| `0x00400020`–end of `.bss`  | `.data` + `.bss`     | Linker `RAM` region; `.data` copied from flash at startup |
| end of `.bss` → bottom of stack | Heap             | `end` symbol; grows upward            |
| `0x43E030`–`0x43FFFF`       | Mode stacks          | See table below; grows downward       |

### Mode stack layout

Stacks sit at the top of RAM Block 1. Each mode's stack is filled with a
canary pattern at boot; overflow detection reads the first word.

| Address range               | Mode    | Size   | Canary       |
|-----------------------------|---------|--------|--------------|
| `0x43FC10`–`0x43FFFF`       | SYS     | 0x3F0  | `0xEEEEEEEE` |
| `0x43F420`–`0x43FC0F`       | FIQ     | 0x7F0  | `0xDDDDDDDD` |
| `0x43E430`–`0x43F41F`       | IRQ     | 0xFF0  | `0xCCCCCCCC` |
| `0x43E040`–`0x43E42F`       | SVC     | 0x3F0  | `0xBBBBBBBB` |
| `0x43E030`–`0x43E03F`       | UNUSED  | 0x010  | `0xAAAAAAAA` |

ABT and UND modes share the UNUSED stack (they are not expected to trigger in
normal operation).

---

## RAM Block 2 (IRAM)

Base: `0x00900000`.  Size: 256 KiB (`0x00900000`–`0x0093FFFF`).

Idle state is `0xAA`-filled. Used by the IRAM build variant (`iram.lds`):
vectors and code load and run from this block, allowing fast firmware iteration
without writing flash. Under normal flash-boot operation this block may be
reserved for RF/MAC or a hardware coprocessor; its use by that subsystem is not
fully characterised.

IRAM firmware is loaded via `tools/iram_loader` over UART and entered directly
at `0x00900000`.

---

## Peripheral Bus (`0x00800000`–`0x008FFFFF`)

All peripheral registers are 32-bit word-aligned. Byte and halfword accesses
to peripheral registers are not supported unless noted.

### Peripheral base addresses

| Base address   | Peripheral    | Driver header              | Notes                                              |
|----------------|---------------|----------------------------|----------------------------------------------------|
| `0x00800000`   | SCTRL         | `hardware/sctrl_regs.h`    | System control: clocks, power, PLL, resets         |
| `0x00800074`   | eFuse         | `hardware/efuse.h`         | Embedded in SCTRL register space (`SCTRL + 0x1D×4`) |
| `0x00802000`   | ICU / INTC    | `hardware/icu.h`, `hardware/intc.h` | Interrupt Controller Unit; clock gating and interrupt routing share this block |
| `0x00802100`   | UART1         | `hardware/uart.h`          | —                                                  |
| `0x00802200`   | UART2         | `hardware/uart.h`          | —                                                  |
| `0x00802300`   | I2C1          | `hardware/i2c.h`           | —                                                  |
| `0x00802480`   | TRNG          | `hardware/random.h`        | True Random Number Generator                       |
| `0x00802600`   | I2C2          | `hardware/i2c.h`           | —                                                  |
| `0x00802800`   | GPIO Bank 0   | `hardware/gpio.h`          | GPIO 0–31 (`+0x00` per pin)                        |
| `0x008028C0`   | GPIO Bank 1   | `hardware/gpio.h`          | GPIO 32+ (`GPIO_BASE + 48×4`)                      |
| `0x00802900`   | WDT           | `hardware/wdt.h`           | Watchdog timer                                     |
| `0x00802A00`   | Timer Bank 0  | `hardware/timer.h`         | 6 timers, 26 MHz source                            |
| `0x00802A40`   | Timer Bank 1  | `hardware/timer.h`         | 6 timers, 32 kHz source (`PWM_NEW + 0x10×4`)       |
| `0x00802C00`   | SAR ADC       | `hardware/saradc.h`        | Successive-approximation ADC                       |
| `0x00803000`   | Flash ctrl    | `hardware/flash.h`         | SPI flash controller (XIP config, erase, program)  |
| `0x00805000`   | FFT           | `hardware/fft.h`           | Hardware FFT accelerator                           |
| `0x00806000`   | Security — AES | `hardware/security.h`     | AES sub-block (`SECURITY_BASE + 0×4`)              |
| `0x00806100`   | Security — SHA | `hardware/security.h`     | SHA sub-block (`SECURITY_BASE + 0x40×4`)           |
| `0x00806200`   | Security — RSA | `hardware/rsa.h`          | RSA sub-block (`SECURITY_BASE + 0x80×4`)           |
| `0x00809000`   | GDMA          | `hardware/gdma.h`          | General-purpose DMA, 6 channels                    |
| `0x0080A000`   | JPEG encoder  | `hardware/jpeg_encoder.h`  | Hardware JPEG encoder                              |

---

## Upper Address Space

### RC — `0x01050000`

RF transceiver control block (`hw_rc`, `rc_regs.h`). Spans
`0x01050000`–`0x010501A8` (107 words). `TRX_REG28` is accessed at the
anomalous address `0x08628078`, outside all mapped regions; the access
mechanism is not characterised.

### MPB — `0x01060000`

MAC PHY Bypass (`hw_mpb`, `mpb_regs.h`). Two sub-blocks: main at
`0x01060000` (`hw_mpb`, `r0`–`r11`) and TX-vector extra at `0x01060200`
(`hw_mpb_extra`, `r128`–`r143`).

### MDM / AGC — `0x01000000`

Modem and AGC registers share a single base address; sub-groups occupy
distinct offset windows:

| Sub-group   | Offset range          | Count | Notes                                                    |
|-------------|-----------------------|-------|----------------------------------------------------------|
| MDM STAT    | `+0x0000`–`+0x0068`   | 27    | Read-only status; `+0x0000` = version register           |
| MDM CFG     | `+0x0800`–`+0x0894`   | 550   | Config registers                                         |
| AGC         | `+0x2000`–`+0x20AC`   | 2092  | Only part of the 2092-register space is described in SDK |
| PHY AGC µcode | `+0xA000`           | —     | SDK comment warns writes to the mirror alias `0x01C0A000` may not be correct on this SoC |

`REG_MDM_CFG_SIZE = 152` and `REG_AGC_SIZE = 172` are SDK-internal constants
that do not reflect total byte sizes (those are COUNT × 4).

### INTC (NX) — `0x10910000`

Interrupt controller for the NX coprocessor subsystem. Size: 68 bytes (17
registers). Distinct from the ARM ICU at `0x00802000`.

### LA — `0x10E00000`

Logic analyzer block. 16 registers. Offset constant `REG_LA_OFFSET = 0x00800000`
suggests a second window or mirror at `0x10E00000 + 0x800000 = 0x11600000`;
purpose not characterised.

### NXMAC — `0xC0000000`

NX 802.11 MAC hardware core. Three sub-blocks:

| Sub-block  | Base           | Size    | Registers | Notes                                    |
|------------|----------------|---------|-----------|------------------------------------------|
| MAC Core   | `0xC0000000`   | 1376 B  | 344       | Core MAC engine                          |
| MAC PL     | `0xC0008000`   | 1404 B  | —         | Physical layer interface                 |
| MAC PTA    | `0xC0010000`   | 36 B    | —         | Packet traffic arbitration; `NXMAC_CONFIG` at `0xC0010004` |

Monotonic free-running counters at `0xC000011C`–`0xC0000124` are used as
microsecond timebase by `hardware_time`.
