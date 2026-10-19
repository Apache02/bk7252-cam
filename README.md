# BK7252 Camera Firmware

Alternative firmware for cameras built on the BK7252U chip (the "A9" mini camera module), written
without the vendor SDK. Work in progress.

After flashing, the camera starts its own Wi-Fi network named `Beken-xxxxxx` (open, no password).
Connect to it and open <http://192.168.4.1/> in a browser. The page has three tabs:

- **Settings** (opens first) — the camera's own network (on/off and name), the Wi-Fi network it
  should join (on/off, name and password), the telnet console and the LED. The camera's MAC
  address is shown here.
- **Camera** — live video.
- **System** — firmware version, reboot, reset the settings.

---

## What you need

- An A9 camera board, version `A9_B_V1_3`. Other boards are not supported.
- A 3.3 V USB–UART adapter connected to UART2 of the board (TX, RX, GND). The images carry `uart2`
  in their name because they use UART2 for the log.
- A Linux shell with Python 3 (on Windows: WSL2). The Python packages are listed in
  `tools/flasher/requirements.txt`.
- The flashing tools from this repository:
  ```sh
  git clone https://github.com/Apache02/bk7252-cam.git
  cd bk7252-cam
  ```
- The firmware image `A9_B_V1_3-camera-<version>-uart2_crc.bin` from the
  [releases page](https://github.com/Apache02/bk7252-cam/releases).

The camera keeps its factory bootloader. Only the application is replaced, so the factory firmware
can always be put back from a backup.

---

## Flashing

`<port>` is the serial port of the adapter. It depends on the system, usually it is `/dev/ttyUSB0`.
The tools use that one by default, so `--port` is only needed when the port is different.

**1. Back up the factory firmware.** This is the only way back, so do not skip it. Keep the file.

```sh
mkdir -p backups
tools/flasher/uartreader --port <port> --segment app -o backups/app_original_crc.bin
```

**2. Flash the camera firmware.**

```sh
tools/flasher/uartprogram --port <port> --segment app A9_B_V1_3-camera-<version>-uart2_crc.bin
```

**3. Open the camera's Wi-Fi network** `Beken-xxxxxx` and go to <http://192.168.4.1/>. The video is
on the **Camera** tab.

### Going back to the factory firmware

```sh
tools/flasher/uartprogram --port <port> --segment app backups/app_original_crc.bin
```

Both tools work with the factory bootloader only. If you installed this project's bootloader
(see [`docs/custom-bootloader.md`](docs/custom-bootloader.md)), use `tools/bkloader flash <file>`
and `tools/bkloader backup` instead.

---

## For developers

Building, running code from RAM, the source layout and the custom bootloader are in
[`docs/development.md`](docs/development.md).

---

## License

MIT — see [`LICENSE`](LICENSE). Vendor binaries and code taken from other projects
keep their own terms; see [`NOTICE`](NOTICE).
