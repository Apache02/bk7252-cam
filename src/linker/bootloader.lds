MEMORY
{
    FLASH(rx) : ORIGIN = 0x00000000, LENGTH = 64k
    RAM(rw)   : ORIGIN = 0x00400020, LENGTH = 256k - 0x20
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

    . = ALIGN(8);

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text.*)
        *(.stub)
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
        _begin_data = .;
        . = . + SIZEOF(.data);
        . = ALIGN(4);
    } > FLASH

    __exidx_end = .;

    /* FAL partition table — scanned by partitions command at [0xd000, 0xf000) */
    .fal_partition_table 0x0000e000 :
    {
        KEEP(*(.fal_partition_table))
    } > FLASH

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

    _data_flash_begin = LOADADDR(.data);
    _data_ram_begin = ADDR(.data);
    _data_ram_end = .;

    .bss ALIGN(8):
    {
        _bss_start = .;
        __bss_start__ = .;
        *(.bss .bss.*)
        *(.scommon)
        *(.sbss)
        *(.dynbss)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
        __bss_end__ = .;
    } > RAM

    .noinit (NOLOAD) :
    {
        *(.noinit)
    } > RAM

    . = ALIGN (8);
    _empty_ram = .;

    PROVIDE(end = .);

    /* _stack symbol defines initial stack bottom addres. Stack grows to lower addresses.
       Typically you set this to be top of your RAM. Note: code never checks, if stack
       grows into heap area!
    */
    PROVIDE(_stack_unused      = 0x440000 - 0x3F0 - 0x7F0 - 0xFF0 - 0x3F0 - 0x10); /* 0x10*/
    PROVIDE(_stack_svc         = 0x440000 - 0x3F0 - 0x7F0 - 0xFF0 - 0x3F0); /* 0x3F0*/
    PROVIDE(_stack_irq         = 0x440000 - 0x3F0 - 0x7F0 - 0xFF0); /* 0xFF0*/
    PROVIDE(_stack_fiq         = 0x440000 - 0x3F0 - 0x7F0); /* 0x7F0*/
    PROVIDE(_stack_sys         = 0x440000 - 0x3F0); /* 0x3F0*/
}
