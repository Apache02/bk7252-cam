MEMORY
{
    IRAM(rwx) : ORIGIN = 0x00900000, LENGTH = 256k
    RAM(rw)   : ORIGIN = 0x00400020, LENGTH = 256k - 0x20
}

OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_vector_start);
_vector_start = ORIGIN(IRAM);


SECTIONS
{
    /* vectors go to start */
    . = ORIGIN(IRAM);
    .vectors :
    {
        KEEP(*(*.vectors))
        KEEP( *(*.rom1))
    } > IRAM

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
    } > IRAM

    .rodata ALIGN(8):
    {
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r*)
        *(.eh_frame)
    } > IRAM


    __exidx_start = .;
    .rwdata ALIGN(8) :
    {
        /* globals.for example: int ram_data[3]={4,5,6}; VMA in RAM, but keep LMA in flash */
        _begin_data = .;
        . = . + SIZEOF(.data);
        . = ALIGN(4);
    } > IRAM

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
        __bss_start__ = .;
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
        __bss_end__ = .;
    } > RAM

    . = ALIGN (8);
    _empty_ram = .;

    /* This symbol defines end of code/data sections. Heap starts here. */
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
