#include "shell/commands_iram.h"
#include "shell/Parser.h"
#include "shell/console_colors.h"
#include "xmodem/xmodem.h"
#include <stdio.h>
#include <stdint.h>
#include "utils/crc32.h"
#include "utils/busy_wait.h"

extern "C" int xmodem_getc(int timeout_ms) {
    for (int i = 0; i < timeout_ms * 200; i++) {
        int c = getchar();
        if (c >= 0) return c;
    }

    return -1;
}

extern "C" void xmodem_putc(uint8_t c) { putchar(c); }

static bool inline valid_ram(uint32_t addr) {
    return (addr >= 0x00400000 && addr < 0x00400000 + 0x00040000) ||
           (addr >= 0x00900000 && addr < 0x00900000 + 0x00040000);
}

int command_iram_xmodem(int argc, const char *argv[]) {
    if (argc < 2 || argc > 3) {
        printf(COLOR_RED("Usage: iram_xmodem <addr> [max_size]") "\r\n");
        return 1;
    }

    uint32_t addr = static_cast<uint32_t>(take_int(argv[1]).ok_or(0));

    if (!valid_ram(addr)) {
        printf(COLOR_RED("Invalid address 0x%08lx") "\r\n", addr);
        return 1;
    }

    uint32_t region_end = (addr >= 0x00900000u) ? (0x00900000u + 0x00040000u) : (0x00400000u + 0x00040000u);
    uint32_t max_size   = region_end - addr;
    if (argc == 3) {
        uint32_t arg_size = static_cast<uint32_t>(take_int(argv[2]).ok_or(0));
        if (arg_size == 0 || arg_size > max_size) {
            printf(COLOR_RED("Invalid max_size") "\r\n");
            return 1;
        }
        max_size = arg_size;
    }

    uint8_t *dest = reinterpret_cast<uint8_t *>(addr);

    printf("Ready for XMODEM transfer to 0x%08lx (max %lu bytes)\r\n", addr, max_size);
    printf("Start sending from your terminal (sx / minicom / TeraTerm)...\r\n");

    busy_wait_ms(200);

    int32_t received = xmodem_receive(dest, max_size);

    if (received < 0) {
        const char *reason = "unknown error";
        switch (received) {
            case XMODEM_ERR_TIMEOUT:
                reason = "timeout";
                break;
            case XMODEM_ERR_CRC:
                reason = "CRC error";
                break;
            case XMODEM_ERR_SEQ:
                reason = "sequence error";
                break;
            case XMODEM_ERR_ABORT:
                reason = "aborted by sender";
                break;
            case XMODEM_ERR_OVERFLOW:
                reason = "buffer overflow";
                break;
            case XMODEM_ERR_NAK:
                reason = "too many NAKs";
                break;
        }
        printf(COLOR_RED("\r\nXMODEM failed: %s (%ld)") "\r\n", reason, received);
        return 1;
    }

    uint32_t checksum = crc32(dest, static_cast<uint32_t>(received));

    printf("\r\nDONE %08lx  (%ld bytes at 0x%08lx)\r\n", checksum, received, addr);

    return 0;
}
