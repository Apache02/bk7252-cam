add_executable(
        bootloader
        main.cpp
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
        utils_busy_wait
        utils_crc
)

bk_bootloader(bootloader)
bk_firmware_iram(bootloader)