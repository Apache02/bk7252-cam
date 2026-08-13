#include "platform/stacks.h"

MEMORY
{
    FLASH    (rx) : ORIGIN = 0x00010000, LENGTH = 1215k
    RAM    (rw!x) : ORIGIN = 0x00400020, LENGTH = 256k - 0x20
    /* Undocumented for this chip variant; confirmed by on-chip probing to be
       readable/writable/executable, single-cycle-like (faster than IRAM).
       See docs/memory_map.md "Upper SRAM Bank". Declared only — unused. */
    TCM   (rwx) : ORIGIN = 0x003F8000, LENGTH = 32k
}

OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_vector_start);
_vector_start = ORIGIN(FLASH);


SECTIONS
{
    /* vectors go to start */
    . = ORIGIN(FLASH);
    .vectors :
    {
        KEEP(*(*.vectors))
        KEEP( *(*.rom1))
    } > FLASH

    /* instructions go to the text region*/

    . = ALIGN(8);

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text.*)
        *(.stub)
        /* .gnu.warning sections are handled specially by elf32.em.  */
        *(.gnu.warning)
        *(.gnu.linkonce.t*)
        *(.glue_7t) *(.glue_7)

        KEEP(*(.fini))

        . = ALIGN(4);
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP(*(SORT(.preinit_array.*)))
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN (__preinit_array_end = .);

        . = ALIGN(4);
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN (__init_array_end = .);

        . = ALIGN(4);
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE_HIDDEN (__fini_array_end = .);

        . = ALIGN(4);
    } > FLASH

    .rodata ALIGN(8):
    {
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r*)
        *(.eh_frame)
    } > FLASH


    __exidx_start = .;
    .rwdata ALIGN(8) :
    {
        /* globals.for example: int ram_data[3]={4,5,6}; VMA in RAM, but keep LMA in flash */
        _begin_data = .;
        . = . + SIZEOF(.data);
        . = ALIGN(4);
    } > FLASH

    __exidx_end = .;

    .data : AT ( _begin_data )
    {
        *(.data .data.*)
        *(.sdata)

        . = ALIGN(4);
        KEEP(*(.jcr*))
        . = ALIGN(4);

        *(.gnu.linkonce.d*)
        SORT(CONSTRUCTORS)
    } > RAM

    _end_data = .;

    /* Loader will copy data from _flash_begin to _ram_begin..ram_end */
    _data_flash_begin = LOADADDR(.data);
    _data_ram_begin = ADDR(.data);
    _data_ram_end = .;

    /* uninitialized data section - global   int i; */
    .bss ALIGN(8):
    {
        _bss_start = .;
        *(.bss .bss.*)
        *(.scommon)
        *(.sbss)
        *(.dynbss)
        *(COMMON)
        /* Align here to ensure that the .bss section occupies space up to
           _end.  Align after .bss to ensure correct alignment even if the
           .bss section disappears because there are no input sections.  */
        . = ALIGN(4);
        _bss_end = .;
    } > RAM

    . = ALIGN (8);
    _empty_ram_begin = .;

    /* This symbol defines end of code/data sections. Heap starts here. */
    PROVIDE(end = .);

    /* Mode stacks, pinned to the top of RAM. Sizes shared with boot_reset.S
       via platform/stacks.h. Nothing checks a stack against the heap. */
    _stack_end = ORIGIN(RAM) + LENGTH(RAM);
    .stacks (_stack_end - _STACK_TOTAL_SIZE_) (NOLOAD) :
    {
        _stack_begin = .;
        _empty_ram_end = .;
        KEEP(*(.stack.unused))
        KEEP(*(.stack.svc))
        KEEP(*(.stack.irq))
        KEEP(*(.stack.fiq))
        KEEP(*(.stack.sys))
    } > RAM
}
