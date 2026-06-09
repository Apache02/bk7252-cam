# Flash Partition Layout

Standard partition layout for the BK7252-based A9 camera (2 MB flash, EN25QH16B).
Derived from the vendor FAL partition table read off hardware.

## Address conventions

Two address spaces coexist:

| FAL flash device      | Address type  | Notes |
|-----------------------|---------------|-------|
| `beken_onchip_crc`    | XIP logical   | CPU-visible address; CRC bytes stripped by hardware |
| `beken_onchip`        | Physical flash | Raw SPI byte offset |

Converting XIP logical address `L` to physical byte offset `P`:

```
P = (L / 32) * 34 + (L % 32)
```

See `docs/hardware/flash.md` — *XIP path* section for full details.

## Standard partition table

| name           | flash device        | start (device offset) | end          | size     |
|----------------|---------------------|-----------------------|--------------|----------|
| `bootloader`   | `beken_onchip_crc`  | `0x000000`            | `0x00FFFF`   | 64 KB    |
| `app`          | `beken_onchip_crc`  | `0x010000`            | `0x11FFFF`   | 1088 KB  |
| `download`     | `beken_onchip`      | `0x132000`            | `0x1DFFFF`   | 696 KB   |

### Why `download` starts at `0x132000`

`app` occupies XIP logical `0x010000–0x11FFFF`. The exclusive end is `0x120000`.
Converting to physical:

```
(0x120000 / 32) * 34 = 0x9000 * 34 = 0x132000
```

No gap — `download` starts on the first physical byte after the CRC-wrapped `app` region.

### Physical ranges for reference

| name         | physical start | physical end | physical size |
|--------------|---------------|--------------|---------------|
| `bootloader` | `0x000000`    | `0x010FFF`   | 68 KB         |
| `app`        | `0x011000`    | `0x131FFF`   | ~1.13 MB      |
| `download`   | `0x132000`    | `0x1DFFFF`   | 696 KB        |
| `unused`     | `0x1E0000`    | `0x1FFFFF`   | 128 KB        |
| **total**    |               |              | **2 MB**      |

### Partition roles

- **`bootloader`** — primary bootloader; XIP-executed from `0x000000`. The vendor
  implementation uses a binary UART protocol.
- **`app`** — main OS firmware; XIP-executed from `0x010000`.
- **`download`** — OTA staging area; raw binary stored here before the bootloader
  copies it to `app`. Addressed via `beken_onchip` (physical).
- **`unused`** — reserved; no current use.

## Flasher segment names

`uartprogram --segment <name>` writes to physical flash:

| segment      | physical start | physical end  | content                       |
|--------------|----------------|---------------|-------------------------------|
| `bootloader` | `0x000000`     | `0x010FFF`    | CRC-wrapped bootloader binary |
| `app`        | `0x011000`     | `0x131FFF`    | CRC-wrapped app binary        |
