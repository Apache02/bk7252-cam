All images are CRC-wrapped (flashable as is). Use `tools/uncrc` to get the raw image.

- `bootloader_a9_v720_crc.bin` - dumped from an A9 camera used with the V720 app
- `bootloader_bk7251_uart2_v1.0.15_crc.bin` - BK7251, log on UART2; found in another
  project, verified on this chip
- `bootloader_bk7251_uart1_v1.0.15_crc.bin` - the same image with log on UART1;
  generated with `tools/crc`, not tested
- `bootloader_v0.1.0-2-g5ae564b-dirty_crc.bin` - this project's own bootloader
  (`src/applications/bootloader`), v0.1.0 plus 2 commits (g5ae564b), built with local
  changes; works with `tools/bkloader` on the board and in the emulator
