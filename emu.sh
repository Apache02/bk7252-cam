#!/bin/sh

BOOTLOADER=bootloaders/bootloader_a9_v720.bin
KERNEL=build/src/applications/shell/shell

qemu-system-arm \
    -M versatilepb \
    -cpu arm926 \
    -m 128M \
    -kernel $KERNEL \
    -device loader,file=$BOOTLOADER,addr=0x00000000,force-raw=on \
    -nographic \
    -audiodev none,id=snd0 \
    -S \
    -gdb tcp::1234
