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

execute_process(
        COMMAND git -C ${CMAKE_CURRENT_LIST_DIR} describe --always --dirty
        OUTPUT_VARIABLE BOOTLOADER_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
)
if (NOT BOOTLOADER_VERSION)
    set(BOOTLOADER_VERSION "unknown")
endif ()

target_compile_definitions(
        bootloader PRIVATE
        ENTRY_BACKUP_REGISTERS
        BOOTLOADER_VERSION="${BOOTLOADER_VERSION}"
)

bk_bootloader(bootloader)
bk_firmware_iram(bootloader)
