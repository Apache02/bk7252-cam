add_executable(
        bootloader
        main.cpp
        version.c
        bootloader_init.c
        shell_handlers.cpp
        partition_table.c
)

target_link_libraries(
        bootloader PRIVATE
        platform_boot
        shell_main
        shell_commands_common
        shell_commands_beken
        shell_commands_iram
        platform_nosys
        platform_stdio_uart2
        hardware_sctrl
        hardware_intc
        hardware_wdt
        hardware_random
        utils_busy_wait
        utils_crc
        platform_fal
)

target_compile_definitions(
        bootloader PRIVATE
        ENTRY_BACKUP_REGISTERS
)

set_source_files_properties(version.c PROPERTIES COMPILE_DEFINITIONS BK_VERSION="${BK_VERSION}")

bk_bootloader(bootloader)
bk_firmware_iram(bootloader)
