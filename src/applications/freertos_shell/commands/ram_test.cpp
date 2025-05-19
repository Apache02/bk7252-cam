#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <stdio.h>
#include "shell/Console.h"
#include "hardware/time.h"


#define count_of(x)     (sizeof(x) / sizeof(x[0]))

#define RAM2_ADDR       (0x00900000)
#define RAM2_SIZE       (0x00040000)
#define TEST_SIZE       (64 * 1024)


const char progress[] = {'/', '-', '\\'};

void command_ram2_rw(Console &c) {
    uint32_t *arr = (uint32_t *) RAM2_ADDR;
    printf("addr 0x%08lx\r\n", (uint32_t) arr);

    uint32_t count = RAM2_SIZE / sizeof(arr[0]);
    printf("write %lu ...  ", count);
    for (uint32_t i = 0, pi = 0; i < count; i++) {
        arr[i] = reinterpret_cast<uint32_t>(&arr[i]);

        if ((i & 0b1111) == 0b1111) {
            printf("\b%c", progress[pi++ % count_of(progress)]);
        }
    }
    printf("\bdone.\r\n");


    uint32_t errorsCount = 0;
    printf("check %lu ...  ", count);
    for (uint32_t i = 0, pi = 0; i < count; i++) {
        if (arr[i] != reinterpret_cast<uint32_t>(&arr[i])) {
            errorsCount++;
        }

        if ((i & 0b1111) == 0b1111) {
            printf("\b%c", progress[pi++ % count_of(progress)]);
        }
    }
    printf("\bdone.\r\n");
    printf("errors count: %lu\r\n", errorsCount);
}


void benchmark_memory(const char *label, uint8_t *ptr, size_t size) {
    // === Write Benchmark ===
    absolute_time_t start_write = get_absolute_time();
    for (size_t i = 0; i < size; ++i) {
        ptr[i] = (uint8_t) (i & 0xFF);
    }
    absolute_time_t end_write = get_absolute_time();
    int64_t write_time_us = absolute_time_diff_us(start_write, end_write);

    // === Read Benchmark ===
    volatile uint32_t checksum = 0;
    absolute_time_t start_read = get_absolute_time();
    for (size_t i = 0; i < size; ++i) {
        checksum += ptr[i];
    }
    absolute_time_t end_read = get_absolute_time();
    int64_t read_time_us = absolute_time_diff_us(start_read, end_read);

    uint32_t write_kbs = (write_time_us == 0) ? 0 : (uint32_t)(((uint64_t)size * 1000) / write_time_us);
    uint32_t read_kbs  = (read_time_us == 0)  ? 0 : (uint32_t)(((uint64_t)size * 1000) / read_time_us);

    printf("== %s ==\r\n", label);
    printf("Write time: %lu us (%lu KB/s)\r\n", (unsigned long)write_time_us, (unsigned long)write_kbs);
    printf("Read  time: %lu us (%lu KB/s)\r\n", (unsigned long)read_time_us,  (unsigned long)read_kbs);
    printf("Checksum: %lu\r\n\r\n", checksum);
}

void command_ram2_speed2(Console &c) {
    uint8_t* sys_mem = (uint8_t*)pvPortMalloc(TEST_SIZE);
    if (!sys_mem) {
        printf("Failed to allocate system memory\n");
        return;
    }

    uint8_t* raw_mem = (uint8_t *)RAM2_ADDR;

    benchmark_memory("System-allocated memory", sys_mem, TEST_SIZE);
    benchmark_memory("Raw memory at 0x00900000", raw_mem, TEST_SIZE);

    vPortFree(sys_mem);
}
