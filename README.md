# BK7252 camera

Test

## Requirements

* gcc-arm-none-eabi
* cmake
* make
* tio
* https://github.com/tiancj/hid_download_py

## Build


## Backup

```shell
uartprogram -r 0x00011000_app_$(date +%Y-%m-%d_%H%M%S)_crc.bin
```

## Flash

```shell
uartprogram -w src/build/hello-world_crc.bin
```

## UART Monitor

```shell
tio -b 115200 /dev/ttyUSB0
```

