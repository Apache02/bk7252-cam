#!/bin/sh

DEVICE=/dev/ttyUSB0

mkdir -p backups
tools/flasher/uartreader -p $DEVICE --segment app -o backups/app_$(date +%Y-%m-%d_%H%M%S)_crc.bin
