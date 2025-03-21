#include "shell/commands_beken.h"
#include <stdio.h>
#include "hardware/flash.h"

#define count_of(x)                 (sizeof(x) / sizeof(x[0]))
#define DUMP_DEFAULT_ADDRESS        0x00000000


template<typename T>
static void print_dump(const T *buf, uint32_t count) {
    char format[] = "%00x ";
    format[2] = '0' + (sizeof(buf[0]) * 2);

    for (size_t i = 0, offset = 0; i < count; ++i) {
        printf(format, buf[i]);

        offset += sizeof(T);
        if (offset % 16 == 0) {
            printf("\r\n");
        }
    }
}

template<typename T>
void command_flash_dump(__unused Console &c) {
    auto addr = c.packet.take_int().ok_or(DUMP_DEFAULT_ADDRESS);
    printf("addr 0x%08x\r\n", addr);

    T buf[256 / sizeof(T)] = {0};
    flash_read(addr, (uint8_t *) buf, sizeof(buf));

    print_dump(buf, count_of(buf));
}

template void command_flash_dump<uint8_t>(Console &c);

template void command_flash_dump<uint32_t>(Console &c);
