# BK7252 emulator

`tools/emulator/` runs this project's firmware on a PC, in a terminal, without the board.
It emulates the CPU with Unicorn and the peripherals with small Python modules. The chip is
described by a profile file; images are placed by command-line arguments.

## Setup

```sh
python3 -m venv tools/emulator/.venv
tools/emulator/.venv/bin/pip install -r tools/emulator/requirements.txt
```

`pyserial` is needed only to bridge a UART to a real host serial device.

## Run

```sh
tools/emulator/run BK7252 \
    --load bootloader:build-release/out/bootloader_crc.bin \
    --load app:build/app_crc.bin
```

The launcher uses the venv automatically. On start it prints the host end of each UART:

```
UART1: /dev/pts/6
UART2: /dev/pts/8
```

Open the UART2 line like a board: `tio -b 115200 /dev/pts/8`. Stop with Ctrl-C.

Options:

| Option | Meaning |
| ------ | ------- |
| `PROFILE` | Profile name, upper case: `profiles/PROFILE.toml`. |
| `--load TARGET:FILE` | Copy FILE byte for byte to TARGET. Repeatable, at least one is required. |
| `--i2c1 TYPE[@ADDRESS]` | Put a device on the I2C1 bus. Repeatable. `TYPE` is a name from `tools/emulator/i2c_devices/`: `hi704`, the camera sensor, 256 registers, id `0x96` in register `0x04`, address `0x30`. `ADDRESS` is a 7-bit number, `0x08`-`0x77`. A device is a 256-byte memory: the first byte of a write sets the offset, every further byte is stored there, and each byte written or read moves the offset on by one. The offset survives a STOP. |
| `-p`, `--publish [HOST_IP:]HOST_PORT:GUEST_PORT[/tcp\|/udp]` | Forward a host port to a port of the firmware, written like Docker's `-p`. Repeatable. See [Network](#network). |
| `-d`, `--debug MODULE[,MODULE...]` | Print debug logs (`gdma`, `flash`, `wdt`, `jpeg`, `net`, `wifi`, or `wifi.rc` for one WiFi block). Several names with commas, or repeat the option: `-d gdma,wifi`. `-d all` turns on every module. |
| `--uart1 SPEC`, `--uart2 SPEC` | Host end of the UART: `pty` (default), `stdio`, or a serial device path. |

Log lines go to stderr. Info, warning and error lines always show; a module's debug lines only with `-d` for it. When stderr is a terminal, info lines are green, warnings yellow and errors red; `NO_COLOR=1`
turns the colour off and `FORCE_COLOR=1` turns it on for a pipe or a file.

`TARGET` is an alias from the profile (case-insensitive) or a number. Aliases of `BK7252`:

| Alias | Where |
| ----- | ----- |
| `RAM`, `IRAM` | RAM block 1 and RAM block 2 |
| `bootloader`, `app` | Raw flash offsets `0x0` and `0x11000` |

Flash aliases point at **raw physical** flash, so a `*_crc.bin` file is loaded as it is. A
flash load must start on a 34-byte block boundary; it can be any length, and the rest of the
flash stays erased (`0xFF`). A full 2 MiB dump loads at offset 0, so use
`--load bootloader:full_dump.bin`; a numeric address `0x0` is the XIP window and is refused. Numeric
addresses are accepted only inside RAM; loading into a module's range or outside memory is an
error reported before the emulator starts.

## Network

With `-p` the emulator puts a virtual wifi station next to the firmware's access point. It
joins the open network, takes an address from the firmware's DHCP server, and forwards the
published host ports to the firmware:

```sh
tools/emulator/run BK7252 --load ... -p 8080:80 -p 2323:23 -p 5000:5000/udp
curl http://127.0.0.1:8080/        # the firmware's HTTP server
telnet 127.0.0.1 2323              # its shell
```

| Form | Meaning |
| ---- | ------- |
| `-p 8080:80` | TCP, host `127.0.0.1:8080` to firmware port 80 |
| `-p 5000:5000/udp` | UDP; add `/tcp` to be explicit |
| `-p 0.0.0.0:8080:80` | Listen on all host interfaces (the default is `127.0.0.1`, unlike Docker, because the firmware's telnet has no password) |

Every TCP client gets its own connection to the firmware. A client that connects before the
firmware has an address waits for it (up to 30 s of device time). Each UDP client address
gets its own firmware-side port, forgotten after 60 s of device time without traffic; the
replies go back to that client. `-d net` logs the join, the lease and every connection.

Only the case of the firmware as an access point is covered: the station joins an open
network, there is one station, the firmware cannot reach the host's network, and a chip reset
(`reboot`, watchdog) drops all connections and joins again. Frames cross the air in the
clear; encryption is not modelled.

## What is emulated

| Block | Notes |
| ----- | ----- |
| CPU | Unicorn, ARM946 model (closest to ARM968E-S). SWI, undefined instruction, aborts, IRQ and FIQ are entered the way the core does. |
| RAM | `TCM` (the 32 KiB upper SRAM bank at `0x003F8000`, below `RAM1`; executable), `RAM1`, `RAM2`; all three read `0xAA` after a chip reset. Code cannot run from `RAM1`: a fetch there logs `bus error: fetch from non-executable memory` and raises a Prefetch Abort, as on the chip (`executable = false` in the profile). Data access works. |
| Flash | 2 MiB raw store and the CRC-decoded XIP window. A block with a bad CRC logs an `ALERT` line and raises a Data Abort while `conf.crc_en` is set. Controller commands: read ID, read and write status registers, write enable/disable, 32-byte read, 32-byte page program (clears bits only), sector, 32 KB and 64 KB block erase, chip erase. The write rules follow `docs/hardware/flash.md`: a command needs `WP#` released; the controller sends the write enable itself, and an explicit one is used up by the command; block-protection bits in the status register stop programs and erases (a rejected command logs a warning). Changes show up in the XIP window at once. So `tools/bkloader flash` and a reboot into the written app work. The contents live in memory only and are lost when the emulator exits. `--debug flash` prints every erase, program, status-register write and write-enable change, decoded (reads are not logged). |
| SCTRL | `chip_id` and `device_id` (read-only, set in the profile); every other register reads back what was written, starting from the profile's `reset_words` (`warm_reset_words` after a chip reset). No analog or clock behaviour. A profile hack (`read_adjust`) makes the MCLK divider (`control[7:4]`) read one higher than written, so busy-wait loops, which count iterations from it, run shorter in emulated time (the emulator is several times slower than the chip). A firmware read-modify-write of that register stores the biased value back, so the divider drifts up to its maximum; `cpu_speed` and `mclk` show the biased frequency. |
| GDMA | Six channels, memory to memory, with the byte-accumulator model of `docs/hardware/dma.md` (widths, +4 address steps, zero-padded tail) and its transfer times. The copy lands when the modelled time has passed; `enable` clears, the finish and half-finish flags and the interrupt follow. A channel whose source is the JPEG request line moves a word whenever the JPEG block has one (one word per 7.2 bus cycles) and waits while it has none; `dst_wr_addr` keeps its position after the channel is stopped. Other peripheral request lines stall and repeat mode runs once, both with a warning. A bus error aborts the channel with an error line. `--debug gdma` prints every configuration, start, completion and acknowledge decoded. |
| WiFi blocks | The register banks of `src/platform/soc/include/rwnx`, one module each in `modules/wifi/`: MAC (core and PL blocks together), MAC PTA, modem config and status, AGC, AGC microcode RAM (`0x0100A000`, 2 KiB), RC, NX interrupt controller, NX DMA, LA. Most are plain memory (what is written reads back, bytes and halfwords included) with no behaviour, so status bits that hardware would clear stay as written. The MAC has behaviour, modelled after the RivieraWaves interface the firmware drives: the state register (`state_cntrl`; leaving a state for IDLE raises the IDLE interrupt, which the firmware waits for before it starts an AP), the general and TX/RX interrupt registers and their six ICU FIQ lines (17-22), the ten ABS timers, the TSF, the TBTT interrupt, the key storage RAM and the time-on-air calculator. The monotonic counters (`monotonic_counter_1` at 40 MHz, `monotonic_counter_2_lo/_hi` in microseconds) run while the MAC is powered (the SCTRL power register holds the wake key) and read 0 otherwise. The MAC PL soft-reset bit (`mac_cntrl_2`, +0x50) clears itself at the next tick. Frames: a TX queue kick walks the descriptor chain and hands each frame to the air, and a little later marks it done and raises the queue's trigger; the receive chain takes frames from the air, with the TSF and the sender's key index, while the MAC is ACTIVE. The air is the virtual station of [Network](#network); without `-p` nobody answers. `--debug wifi` logs every read and write of every WiFi block; `--debug wifi.mac_core` picks one block. |
| GPIO | 40 pins in two banks. A pin drives its pad while its output driver is on; otherwise the pad follows the level forced from the host side, then the pull resistor, then reads 0. Interrupts: level low/high, rising/falling edge. Peripheral functions of a pin are not modelled. |
| TRNG | Not random, so runs are repeatable: `data` returns a read counter (0, 1, 2, ...) and `ctrl.enable` is 1 from the start and after a chip reset. While disabled, `data` is a plain latch, as in `docs/hardware/random.md`. |
| I2C1 | The controller of `docs/hardware/i2c.md`: one byte per bus phase, `si` and the ICU line (bit 2) come up after nine bit times of the programmed bus clock, `busy` follows START and STOP. `sto` is a one-shot command here and reads back 0 (the doc describes it as latched). The devices on the bus are chosen with `--i2c1 TYPE[@ADDRESS]`; with none, every address is a NACK. I2C2 is not emulated. `-d i2c` logs each register write and bus phase. |
| JPEG | The encoder and DVP block of `docs/hardware/jpeg.md`, with no sensor behind it. While `enc_en` is set, a frame starts every second (`start_frame`: the output FIFO fills with a finished JPEG and its read position goes back to the first word, `empty_fifo` clears) and ends 150 ms later (`end_frame`, `byte_cnt_pfrm`); the intervals are the profile's `frame_interval_ms` and `encode_ms`. Both flags are W1C and drive FIQ source 29 through `ctrl0`. `rx_fifo_data` gives the stream a word per read, byte-reversed unless `video_byte_rev` is set, and `empty_fifo` returns once it is drained; reading it empty gives the last frame's size with `enc_size`, else 0. With `enc_size` the stream ends with a zero byte and the 32-bit size, and the file is padded (with a comment segment) so that it plus that trailer fills whole words. A DMA channel with the JPEG request line (8) drains the FIFO. The picture is the file `tools/emulator/modules/jpeg/picture.jpg` (640x480, colour bars over a grey ramp), read when the emulator starts; replace it to show something else. Picture size, quantization table, sync polarity, input format and bit-rate control do nothing. `-d jpeg` logs the frame boundaries. |
| SAR ADC | `0x00802C00`, layout in `soc/saradc.h`. While `config.chnl_en` is set, a sample arrives every 100 us of device time (value `sample` in the profile, 330, the same on every channel) into a 32-sample FIFO; the conversion timing fields do nothing. `data` and `dat_after_sta` both pop a sample (the driver's ISR drains through `dat_after_sta`), `fifo_empty` and `fifo_full` follow the FIFO, and `int_clr` is a command that reads back 0. A sample raises ICU source 11 once `ctrl_cfg.almost_cfg` samples wait (one if it is 0); the flag stays until `int_clr`. Clearing `chnl_en` stops the conversion and empties the FIFO. |
| unknown | A block nobody has described yet, for any address (`type = "unknown"`, `size`, optional `reset_words`). It is plain memory, and every read and write is an INFO line with the block name, offset, value, and the address and opcode of the instruction that made the access (`INFO emulator.unknown: MDM_3000 read +0x0000 (4 bytes) = 0x00274C10  pc 0x0009D444 insn 6811`; Thumb opcodes show four hex digits). The profile uses it for the block at `0x01003000`, which no SDK header describes, with the words read from a running chip. |
| ICU | Interrupt enable, status, global IRQ/FIQ enables. |
| UART1, UART2 | 128-byte RX FIFO, instant TX, level and latched interrupts. `rx_need_read` follows the FIFO threshold; `rx_stop_end` is latched 1 ms (device time) after the last received byte, so input shorter than the threshold still reaches the firmware. There is no baud rate: `rx_stop_detect_time` is ignored and parity and stop errors never happen. |
| Timers | Two banks (26 MHz and 32 kHz), three periodic timers each. |
| WDT | Period in ms and the `0x5A`/`0xA5` reload keys. Counts only while the ICU clock-power-down bit for it is 0 (it is 1 at reset). Expiry resets the chip. `-d wdt` logs every reload ("kick"): the period, the time since the previous kick as a percent of its period (close to 100% means the chip nearly reset) and how far the reset moved. |

A chip reset (watchdog expiry, so also the shell `reboot` command) fills every memory region that has a
`reset_fill` in the profile with that byte (both RAM blocks of BK7252 use `0xAA`; a region without
it keeps its contents), clears the general registers, returns every module to its reset state, keeps the flash contents and
restarts the CPU at address 0. The UART endpoints stay connected.

Anything else in the peripheral window (`0x00800000`-`0x008FFFFF`) is logged once per address as
`unclaimed peripheral` and reads as 0. An access to memory nothing answers logs
`bus error: unmapped access` and raises a Data or Prefetch Abort.

Time is a virtual clock driven by executed instructions. When the CPU waits for an interrupt
the clock jumps to the next timer event.

## Profile

`profiles/<NAME>.toml` holds the instruction rate, the peripheral window, `[[memory]]` regions (each with an optional `reset_fill` byte and an `executable` flag, true by default),
`[alias]` entries (a number is a bus address; a table `{module, offset}` points into a module)
and `[[module]]` instances (`type`, `name`, `[module.bases]` per address range, `[module.params]`).
A module that raises interrupts names its controller and source bit with `intc` and `irq`; the
controller must be listed first. A module that watches a register bit of another module takes a
param ending in `_signal`, such as `clock_off_signal = "ICU.arm_wdt_pwd"`; the owner declares the
bit under its own `signals` param. Every module also gets `clock` and `reset_chip` constructor
arguments.

## Adding an I2C device

1. Write `tools/emulator/i2c_devices/<name>.py`: subclass `I2cDevice` (`device.py`), or
   `RegisterDevice` (`register_device.py`) for a device that is a block of registers. Set
   `NAME` and `DEFAULT_ADDRESS`.
2. Add the class to `DEVICES` in `i2c_devices/__init__.py`.

## Adding a module

1. Write `tools/emulator/modules/<name>.py`: subclass `core.module.Module`, return
   `(range_name, size)` pairs from `ranges()`, implement `read`/`write`, and `tick`/`next_event_ns`
   if it depends on time. Take collaborators (`clock`, `irq`, `endpoint`, ...) as constructor
   arguments.
2. Register the class in `modules/__init__.py`.
3. Add a `[[module]]` entry to the profile.

The core never imports a module and a module never imports another module.

## Tests

```sh
tools/emulator/.venv/bin/python -m unittest discover -s tools/emulator/tests -t tools/emulator
```

Booting a real image is a manual check.

## Limits

One chip. Flash is not saved between runs. The WiFi blocks other than the MAC are plain
memory, the network covers only the firmware as an open access point, and the camera shows one fixed picture. The GPIO module has a Python interface (`set_input`, `pad_level`, `watch`) for
tests and scripts, but the command line does not expose it; firmware that needs them runs into the log lines above.
