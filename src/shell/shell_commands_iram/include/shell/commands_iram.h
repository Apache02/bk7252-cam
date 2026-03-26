#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Load binary data into memory region via serial port.
// Usage: iram_load <addr_hex> <size>
// Protocol:
//   1. Command prints "READY\r\n"
//   2. Host sends exactly <size> raw bytes
//   3. Command prints "DONE <crc32_hex>\r\n" on success
//      or "ERROR\r\n" on failure
int command_iram_load(int argc, const char *argv[]);


// Disable interrupts and jump to address.
// Usage: iram_jump <addr_hex>
int command_iram_jump(int argc, const char *argv[]);

int command_iram_xmodem(int argc, const char *argv[]);

#ifdef __cplusplus
}
#endif
