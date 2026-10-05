#include "shell/commands_beken.h"
#include "shell/Parser.h"
#include "hardware/flash.h"
#include "utils/busy_wait.h"
#include "utils/crc32.h"
#include <stdio.h>
#include <string.h>


#define CHUNK          (256u)
#define ACK            0x06
#define ACK_TIMEOUT_MS 5000
#define POLL_INTERVAL  10

// Copies count bytes starting at addr into dst.
typedef void (*binary_read_fn)(uint32_t addr, uint8_t *dst, uint32_t count);


static bool has_ack(const uint32_t timeout_ms) {
    int  countdown = timeout_ms / POLL_INTERVAL;
    bool ack       = false;
    while (countdown > 0 && !ack) {
        busy_wait_ms(POLL_INTERVAL);
        int c = getchar();
        if (c == ACK) {
            ack = true;
        } else if (c >= 0) {
            countdown = timeout_ms / POLL_INTERVAL;
        } else {
            countdown--;
        }
    }

    return ack;
}

int binary_stream_command(const int argc, const char *argv[], const binary_read_fn read) {
    if (argc != 3) {
        printf("Usage: %s <addr> <size>\r\n", argv[0]);
        return 1;
    }

    const uint32_t addr = static_cast<uint32_t>(take_int(argv[1]).ok_or(0));
    const uint32_t size = static_cast<uint32_t>(take_int(argv[2]).ok_or(0));

    if (size == 0) {
        printf("Invalid size\r\n");
        return 1;
    }

    printf("Send ACK (0x06) to start binary transfer or wait to abort\r\n");

    if (!has_ack(ACK_TIMEOUT_MS)) {
        printf("Aborted\r\n");
        return 0;
    }

    uint8_t  buf[CHUNK];
    uint32_t crc = crc32_init();

    for (uint32_t off = 0; off < size; off += CHUNK) {
        uint32_t n = size - off;
        if (n > CHUNK) n = CHUNK;
        read(addr + off, buf, n);
        for (uint32_t i = 0; i < n; i++) putchar(buf[i]);
        crc = crc32_update(crc, buf, n);
    }

    crc = crc32_final(crc);
    putchar(static_cast<uint8_t>(crc & 0xFF));
    putchar(static_cast<uint8_t>((crc >> 8) & 0xFF));
    putchar(static_cast<uint8_t>((crc >> 16) & 0xFF));
    putchar(static_cast<uint8_t>((crc >> 24) & 0xFF));

    return 0;
}

int command_flash_read_binary(int argc, const char *argv[]) { return binary_stream_command(argc, argv, flash_read); }

static void memory_copy(uint32_t addr, uint8_t *dst, uint32_t count) {
    memcpy(dst, reinterpret_cast<const void *>(addr), count);
}

int command_memory_read_binary(int argc, const char *argv[]) { return binary_stream_command(argc, argv, memory_copy); }
