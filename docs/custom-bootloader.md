# Custom bootloader

`src/applications/bootloader` replaces the vendor bootloader in the `bootloader`
flash partition (`0x000000–0x00FFFF`). Instead of the vendor's binary UART
protocol it provides an interactive shell — similar to U-Boot or GRUB.

**What it provides:**
- `loadi` / `go` / `speed` — IRAM load workflow (used by `bkloader`).
- `flash_read` / `flash_write` / `buffer` — raw flash access (used by `bkloader`).
- `reboot` — reboot with autoboot countdown, interruptible for shell access.

**Compatibility:** tested with the original camera firmware — the camera boots and
operates correctly with this bootloader in place.

**After installing:** `tools/flasher/uartprogram` and `uartreader` stop working
(they speak the vendor protocol). Use `tools/bkloader` for all subsequent flashing.

The firmware in the README works with the vendor bootloader. Installing this one is only needed for
development (running code from RAM, `bkloader`).

## Installing

The vendor bootloader cannot erase/write its own partition while executing from it.
The only installation path is via `bootloader_installer` — an IRAM app that runs
entirely from RAM and writes the bootloader partition from there.

From a release, the two images are `ram_loader-<version>-uart2_crc.bin` and
`bootloader_installer-<version>-uart2_iram.bin`:

```sh
# 0. Back up the app partition. Step 1 overwrites it, and this is the only copy of the vendor app.
mkdir -p backups
tools/flasher/uartreader --segment app -o backups/app_original_crc.bin

# 1. Flash ram_loader to the app partition (vendor bootloader protocol).
#    It replaces the app for now: it is a shell with loadi / go / speed.
tools/flasher/uartprogram --segment app ram_loader-<version>-uart2_crc.bin

# 2. Back up the bootloader partition (the installer overwrites it). uartreader cannot read it, but
#    ram_loader is running now and has flash_read.
tools/bkloader backup --segment bootloader --output backups/bootloader_original_crc.bin

# 3. Load and run bootloader_installer from RAM
tools/bkloader iram bootloader_installer-<version>-uart2_iram.bin --capture 15
```

`bootloader_installer` writes the embedded `bootloader_crc.bin` to the bootloader
partition, verifies it and prints `OK`, then reboots the chip through the watchdog, so it starts
the new bootloader. If the verification fails it prints `FAILED` and does not reboot, because the
reset would boot a damaged image. If the same bootloader is already installed it prints
`Already installed.` and does nothing. Building `bootloader_installer--iram`
builds `bootloader` first, so no separate build step is needed.

```sh
# 4. Restore the vendor app (now through the new bootloader).
#    It works with the custom bootloader.
tools/bkloader flash backups/app_original_crc.bin
```

## Restoring the vendor bootloader

Same installer, different payload. Point it at a vendor image instead of the
`bootloader` build. The custom bootloader already has `loadi`, so `ram_loader`
is not needed. The vendor images are in `bootloaders/`; the copy made in *Installing*
(`backups/bootloader_original_crc.bin`) works too.

```sh
# 1. Rebuild the installer with a vendor image embedded (CRC-wrapped file only)
(cd build && cmake -DBOOTLOADER_CRC_BIN=$PWD/../bootloaders/bootloader_bk7251_uart2_v1.0.15_crc.bin .. \
    && make bootloader_installer--iram)

# 2. Run it from RAM
tools/bkloader iram build/app_iram.bin --capture 15
```

The installer writes the vendor image to `0x000000` and reboots (same as above). After that
`uartprogram` and `uartreader` work again, and `bkloader` does not.

With an explicit `BOOTLOADER_CRC_BIN` the installer does not rebuild `bootloader`.
To return to the custom bootloader later, run `cmake -DBOOTLOADER_CRC_BIN= ..`
(empty value) and repeat the install steps.
