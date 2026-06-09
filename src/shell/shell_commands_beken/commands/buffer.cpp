#include "shell/commands_beken.h"
#include "shell/Parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


#define MAX_BUFFER          (256 << 10)

static void *current_buf = nullptr;
static uint32_t current_size = 0;


static bool valid_size(uint32_t size) {
    return size <= MAX_BUFFER && size > 0;
}

int command_buffer(int argc, const char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <size>\r\n", argv[0]);
        return 1;
    }

    uint32_t size = static_cast<uint32_t>(take_int(argv[1]).ok_or(0));
    if (!valid_size(size)) {
        printf("Invalid size\r\n");
        return 1;
    }

    if (current_buf) {
        if (current_size < size) {
            free(current_buf);
            current_buf = nullptr;
            current_size = 0;
        }
    }

    if (!current_buf) {
        current_buf = malloc(size);

        if (!current_buf) {
            printf("Allocation failed\r\n");
            return 1;
        }

        current_size = size;
    }

    printf(
        "addr 0x%08lx  size %lu\r\n",
        reinterpret_cast<unsigned long>(current_buf),
        static_cast<unsigned long>(current_size)
    );
    return 0;
}
