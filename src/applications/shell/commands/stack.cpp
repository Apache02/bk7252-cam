#include "commands.h"
#include <stdio.h>

extern "C" uint8_t *_stack_unused;
extern "C" uint8_t *_stack_svc;
extern "C" uint8_t *_stack_irq;
extern "C" uint8_t *_stack_fiq;
extern "C" uint8_t *_stack_sys;

static const uint8_t *ram_end = (uint8_t *) 0x00440000;

static void print_stack_region(const char *name, uint32_t color, uint32_t addr_start, uint32_t size) {
    int count = size / sizeof(uint32_t);
    uint32_t *ptr = ((uint32_t *) (addr_start + size));
    int used = 0;
    while (ptr[-1 - used] != color && (used < count)) used++;

    char size_s[16];
    sprintf(size_s, "0x%lx", size);
    char used_s[16];
    sprintf(used_s, "0x%x (%d%%)", used * sizeof(uint32_t), (int) (used * sizeof(uint32_t) * 100 / size));

    printf(
            "| %10s | 0x%08lx | %8s | 0x%08lx | %12s |\r\n",
            name, addr_start, size_s, color, used_s
    );
}

void command_stack(Console &c) {
    printf(
            "| %10s | %10s | %8s | %10s | %12s |\r\n",
            "name", "start", "size", "color", "used"
    );
    printf(
            "| %10s | %10s | %8s | %10s | %12s |\r\n",
            "----------", "----------", "--------", "----------", "------------"
    );
    print_stack_region("unused", 0xaaaaaaaa, (uint32_t) &_stack_unused,
                       ((uint32_t) &_stack_svc) - ((uint32_t) &_stack_unused));
    print_stack_region("svc", 0xbbbbbbbb, (uint32_t) &_stack_svc, ((uint32_t) &_stack_irq) - ((uint32_t) &_stack_svc));
    print_stack_region("irq", 0xcccccccc, (uint32_t) &_stack_irq, ((uint32_t) &_stack_fiq) - ((uint32_t) &_stack_irq));
    print_stack_region("fiq", 0xdddddddd, (uint32_t) &_stack_fiq, ((uint32_t) &_stack_sys) - ((uint32_t) &_stack_fiq));
    print_stack_region("sys", 0xeeeeeeee, (uint32_t) &_stack_sys, ((uint32_t) ram_end) - ((uint32_t) &_stack_sys));
}
