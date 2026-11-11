# Wi-Fi port vs. vendor SDK: differences found on the chip

Collected on 2026-11-10/11 while looking for the reason Wi-Fi failed with a slow
AHB clock (see "The one that mattered" below). Both firmwares ran on the same board
(A9_B_V1_3) behind this project's bootloader, joined the same access point on
channel 9, and were compared by reading registers at the same moment of a join
(right after the first frame exchange) and by tracing the MAC state machine.

- **Ours:** `freertos_wifi` from this repository, branch `feat/dco-180mhz`.
- **Vendor:** the `beken7252-opencam` SDK build (`project/rtthread_crc.bin`),
  RT-Thread, clock word `SCTRL.ctrl = 0x00321200`.

Every row says whether the difference was tested and what the test showed.
"No effect" means: the vendor's value was applied to our firmware on a slow bus and
the failure stayed exactly the same.

## What is the same

Worth knowing before reading the differences, because these were all suspects.

- The LMAC archive `libip_7221u.a` is byte-identical in both trees.
- Modem configuration (`0x01000800`, 256 words), AGC (`0x01002000`, 256 words),
  the block at `0x01003000` up to `+0x24`, and the radio controller
  (`0x01050000`, 128 words) read the same, apart from live status fields and the
  TX gain index (below).
- MAC core (`0xC0000000`–`0xC000057F`) and MAC platform (`0xC0008000`–`0xC00083FF`,
  `0xC0008500`–`0xC000857F`) read the same, apart from counters, timers, pointers
  and the items listed below.
- TX header descriptor and policy table for the join probe: same layout, same
  retry limits (`0xFFFF0704`), same rate steps (`0x00000400`, then three zero
  steps). Only the power field differs.
- The AGC microcode table is the same.
- The vendor really runs the CPU at 180.6 MHz from the DCO with `hclk_div2_en` set
  (measured with a fixed instruction loop against the MAC microsecond counter).
  Our DCO setting measures 179.4 MHz.
- The MAC hardware sequence of a failed exchange is the same in both: response
  wait, back to idle, backoff, TX DMA `PASSIVE` → `ACTIVE`, a pre-TX state of about
  3 µs, transmit.

## The one that mattered

| Item | Vendor | Ours (before the fix) | Result |
|---|---|---|---|
| `hal_machw_init_diagnostic_ports()`, a hook the LMAC calls from `hal_machw_init` | writes `0x08` to MAC register `0xC0000700` ("bypass buf fix" in `driver/rw_pub/rw_platf_pub.c`) | empty function | **Root cause.** Fixed in `src/platform/port_wifi/glue/mac_glue.c` |

Without the write, the MAC hangs before every retransmission when the AHB clock is
90 MHz or lower (lossy at 120 MHz, fine at 160 MHz). The first transmission of a
frame always works. With the write, join and ping work at 160 MHz + hclk/2,
DCO 180 MHz + hclk/2, 120 MHz + hclk/2 and 80 MHz.

Register `0xC0000700` is not in the vendor register headers and sits outside the
ranges usually dumped for a MAC comparison, which is why register diffs did not
show it.

## Wi-Fi related, tested, no effect on the slow-bus failure

| Item | Vendor | Ours | Notes |
|---|---|---|---|
| MAC debug port mux `0xC0000510` | `0x0107` (same hook as above) | `0` | Diagnostics only. Not ported |
| TX gain index of the join probe (policy table power field, `TXVECTOR0` `0x01000040`, radio controller `0x01050014`) | `0x0C` | `0x34` | Forced to `0x0C` in our firmware: no effect |
| `NXMAC_MAX_POWER_LEVEL` `0xC00000A0` | `0x0C0C` | `0x3434` | Follows the gain index |
| Power field in the data-frame policy tables | `0x20` | `0x3E` | Poked in RAM: no effect |
| Upper power clamp in `phy_get_rf_gain_idx()` | 10 dBm (`PHY_TRIDENT_LIMIT_PWR`) | 25 dBm (`PHY_MAX_PWR_24G`) | Explains why our index can exceed the vendor's maximum of 32 |
| Order in the system-control init | MAC and modem powered up before the DPLL calibration | after it | Vendor order tried: no effect |
| GPIO 9–13 config (`0x00802824`–`0x00802834`) | `0x00`, then `0x78` ×4 | `0x28` ×5 | Vendor values poked: no effect |
| GPIO 20–23 config (`0x00802850`–`0x0080285C`) | `0x48`, `0x48`, `0x00`, `0x00` | `0x68` ×4 | Vendor values poked: no effect |
| `0x00802898` | `0` | `1` | DPLL-unlock interrupt enable. Poked: no effect |
| `0x008028B8` | `0x5000` | `0x5500` | Poked: no effect |
| `SCTRL.ctrl` | `0x00321200` | `0x00021200` at the same clock | Differs only in the VDDRAM level bits. Vendor word applied: no effect |

`NXMAC_TX_BW_CNTRL` (`0xC0000310`) was once read as `0x20A98` in the vendor and
`0x20A18` in ours. At the same moment of a join both read `0x20A98`, so this is
state-dependent and not a configuration difference.

## Wi-Fi related, not tested

| Item | Vendor | Ours | Notes |
|---|---|---|---|
| How MAC and modem interrupts reach the CPU | registered as FIQ sources | one unified handler table, taken as IRQ | The slow-bus failure happens with the TX trigger interrupt masked, so this was not the cause |
| `0x01003028`, `0x0100302C` | `0x3D29072B`, `0x0000403B` | `0`, `0` | Read during a join. Meaning unknown, looks like status |
| MAC address | from the vendor's own storage | from this project's storage | The two firmwares use different addresses on the same board |
| DPLL 60 MHz without `hclk_div2_en` | not tested | one join failed with "no answer before the deadline" | Seen after the fix. Not investigated |

`CFG_BK7221_MDM_WATCHDOG_PATCH` is 0 in the vendor configuration, so
`rc_reset_patch()` is never called there either. The vendor modem interrupt handler
only acknowledges the interrupt (its `ASSERT_REC` is an empty macro), same as ours.

## Not Wi-Fi: other peripherals the vendor build sets up

Listed so that a future register diff does not chase them.

| Item | Vendor | Ours | Notes |
|---|---|---|---|
| System tick source | PWM block (`0x00802A80 = 0x7`, `0x00802A88 = 0x6590`) | timer bank 1 (`0x00802A48`, `0x00802A4C`) | Different RTOS port |
| ICU interrupt enable `0x00802040` | `0x007F8207` | `0x107F0102` | Vendor also enables UART1, I2C1, PWM, GDMA; ours enables TIMER and DPLL-unlock |
| ICU peripheral clock mux `0x00802000` | `0x197` | `0x083` | |
| ICU peripheral clock power-down `0x00802008` | `0x00FF7DF8` | `0x00CFFEFD` | |
| `SCTRL.block_enable` (`0x0080012C`) | `0x0004057F` | `0x0000057F` | Vendor powers the QSPI / VDDRAM block |
| `SCTRL.analog_ctrl8/9/10` (`0x0080008C`–`0x00800094`) | `0x00335868`, `0xE3A04647`, `0x808011A7` | `0x8033087C`, `0x80000E3F`, `0x808015F2` | Vendor audio setup |
| Audio block `0x00802B00`–`0x00802B34` | configured | all zero | |
| GDMA `0x00809000`… | several channels configured, one pointing at the audio block | all zero | |
| UART1 `0x00802100` | configured | off | |
| I2C1 `0x00802300` | `0x1500` | `0` | |
| Watchdog period `0x00802900` | `0x1388` | `0x07D0` | |

## How the comparison was made

- **State trace.** A low-priority task polls `DEBUG_HWSM_2` (`0xC0000504`, top
  byte is the MAC control state), `DMA_STATUS_1` (`0xC0008188`) and the MAC
  microsecond counter (`0xC0000120`) and logs every change. The same task was built
  into the vendor SDK as a file in `project/applications/`.
- **Same-moment register snapshot.** The task copies whole register blocks when the
  MAC returns to idle after a response wait, so both firmwares are compared in the
  same state, not at an arbitrary idle time.
- **Clock check.** A two-instruction loop (4 CPU cycles per pass) timed against the
  MAC microsecond counter gives the real CPU clock; the same loop with a peripheral
  read shows whether `hclk_div2_en` is in effect.
- **MAC debug port.** Writing a selector to `0xC0000510` and reading `0xC000050C`
  exposes internal signals; selectors `0x00`–`0x31` are distinct. Signal names are
  not known.

MAC control states seen in the traces: `0x40` idle, `0x61`–`0x67` receive,
`0x81`–`0x83` just before a transmission, `0x88` transmit, `0x89` waiting for the
response, `0x8A` taking the response in, `0x8B` closing the exchange.

Trace captures are in `logs/2026-11-11_mactrace*.txt` (not tracked).
