#include "shell/commands_beken.h"
#include <stdio.h>
#include <stdint.h>

extern "C" uint8_t *_stack_unused;
extern "C" uint8_t *_stack_svc;
extern "C" uint8_t *_stack_irq;
extern "C" uint8_t *_stack_fiq;
extern "C" uint8_t *_stack_sys;

static const uint8_t *ram_end = reinterpret_cast<uint8_t *>(0x00440000);

static void print_stack_region(const char *name, uint32_t color, const void *addr_start, size_t size) {
    int count = size / sizeof(uint32_t);
    uint32_t *ptr = reinterpret_cast<uint32_t *>(reinterpret_cast<uint32_t>(addr_start) + size);
    int used = 0;
    while (ptr[-1 - used] != color && (used < count)) used++;

    char size_s[16];
    sprintf(size_s, "0x%lx", static_cast<uint32_t>(size));
    char used_s[16];
    sprintf(used_s, "0x%x (%d%%)", used * sizeof(uint32_t), static_cast<int>(used * sizeof(uint32_t) * 100 / size));

    printf(
        "| %10s | 0x%8p | %8s | 0x%08lx | %12s |\r\n",
        name, addr_start, size_s, color, used_s
    );
}

#define block_size(ptr_start, ptr_end)      reinterpret_cast<unsigned int>(ptr_end) - reinterpret_cast<unsigned int>(ptr_start)

int command_stack(__unused int argc, __unused const char *argv[]) {
    printf(
        "| %10s | %10s | %8s | %10s | %12s |\r\n",
        "name", "start", "size", "color", "used"
    );
    printf(
        "| %10s | %10s | %8s | %10s | %12s |\r\n",
        "----------", "----------", "--------", "----------", "------------"
    );
    print_stack_region("unused", 0xaaaaaaaa, &_stack_unused, block_size(&_stack_svc, &_stack_unused));
    print_stack_region("svc", 0xbbbbbbbb, &_stack_svc, block_size(&_stack_irq, &_stack_svc));
    print_stack_region("irq", 0xcccccccc, &_stack_irq, block_size(&_stack_fiq, &_stack_irq));
    print_stack_region("fiq", 0xdddddddd, &_stack_fiq, block_size(&_stack_sys, &_stack_fiq));
    print_stack_region("sys", 0xeeeeeeee, &_stack_sys, block_size(ram_end, &_stack_sys));

    return 0;
}
