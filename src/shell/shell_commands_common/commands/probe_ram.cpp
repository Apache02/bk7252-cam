#include <stdio.h>
#include <stdint.h>
#include "shell/Parser.h"


/**
 * Returns true if the block is uniform (all words equal).
 */
static bool probe_pass1(uint32_t addr, uint32_t size) {
    auto *ptr = reinterpret_cast<volatile uint32_t *>(addr);
    uint32_t count = size / 4;

    uint32_t run_val = ptr[0];
    uint32_t run_start = addr;
    uint32_t run_bytes = 0;
    bool uniform = true;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t val = ptr[i];
        uint32_t cur_addr = addr + i * 4;

        if (val == run_val) {
            run_bytes += 4;
            continue;
        }

        if (uniform) {
            printf("[pass1] Block is NOT uniform\r\n");
            uniform = false;
        }

        printf("  0x%08lx: 0x%08lx bytes of 0x%08lx  ->  diverges at 0x%08lx = 0x%08lx\r\n",
               run_start,
               run_bytes,
               run_val,
               cur_addr,
               val
        );

        run_start = cur_addr;
        run_val = val;
        run_bytes = 4;
    }

    if (uniform) {
        printf("[pass1] Uniform: 0x%08lx bytes of 0x%08lx starting at 0x%08lx\r\n", size, run_val, addr);
    } else {
        // print last run
        printf("  0x%08lx: 0x%08lx bytes of 0x%08lx  [last run]\r\n", run_start, run_bytes, run_val);
    }

    return uniform;
}

static void probe_pass2(uint32_t addr, uint32_t size) {
    auto *ptr = reinterpret_cast<volatile uint32_t *>(addr);
    uint32_t count = size / 4;

    printf("[pass2] Write-read test (addr -> cell -> restore)...\r\n");

    uint32_t ok_count = 0;
    uint32_t fail_count = 0;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t cur_addr = addr + i * 4;
        // uint32_t orig = ptr[i];

        ptr[i] = cur_addr;
        __asm__ volatile ("" ::: "memory");
        uint32_t readback = ptr[i];

        if (readback == cur_addr) {
            // ptr[i] = orig; // restore
            ok_count++;
        } else {
            if (fail_count == 0) {
                printf("  First failure at 0x%08lx (wrote 0x%08lx, read 0x%08lx)\r\n", cur_addr, cur_addr, readback);
            }
            fail_count++;
        }
    }

    printf("[pass2] %lu / %lu cells writable\r\n", ok_count, count);

    if (fail_count == 0) {
        printf("[pass2] RAM OK  -- 0x%08lx, size 0x%lx\r\n", addr, size);
    } else if (ok_count == 0) {
        printf("[pass2] NOT RAM / read-only\r\n");
    } else {
        printf("[pass2] PARTIAL -- %lu writable, %lu failed\r\n", ok_count, fail_count);
    }
}

static void usage() {
    printf("\r\n");
    printf("Usage: probe <addr> <size>\r\n");
    printf("  addr and size must be multiples of 4\r\n");
    printf("\r\n");
}

int command_probe_ram(int argc, const char *argv[]) {
    if (argc < 3) {
        usage();
        return -1;
    }

    const uint32_t addr = take_int(argv[1]).ok_or(0);
    const uint32_t size = take_int(argv[2]).ok_or(0);

    if (addr % 4 != 0) {
        printf("Error: addr 0x%08lx is not 4-byte aligned\r\n", addr);
        return -1;
    }
    if (size == 0 || size % 4 != 0) {
        printf("Error: size 0x%lx must be non-zero and multiple of 4\r\n", size);
        return -1;
    }

    printf("Probing 0x%08lx .. 0x%08lx (0x%lx bytes)\r\n",
           addr,
           (addr + size - 1),
           size
    );

    bool uniform = probe_pass1(addr, size);

    if (uniform) {
        probe_pass2(addr, size);
    } else {
        printf("[pass2] Skipped -- non-uniform content. Use 'dump32' to inspect before writing.\r\n");
    }

    return 0;
}
