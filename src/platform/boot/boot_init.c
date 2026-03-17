#include <stdint.h>

void _sysboot_zi_init() {
    extern uint32_t _bss_start;
    extern uint32_t _bss_end;

    uint32_t *p = &_bss_start;
    while (p < &_bss_end) {
        *p++ = 0;
    }
}

void _sysboot_copy_data_to_ram(void)
{
    extern uint32_t _data_flash_begin;
    extern uint32_t _data_ram_begin;
    extern uint32_t _data_ram_end;

    uint32_t *src = &_data_flash_begin;
    uint32_t *dst = &_data_ram_begin;
    while (dst < &_data_ram_end) {
        *dst++ = *src++;
    }
}
