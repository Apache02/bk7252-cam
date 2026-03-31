add_executable(
        freertos_shell
        main.cpp
        shell_task.cpp
        shell_handlers.cpp
)

target_include_directories(
        freertos_shell PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/config
)

file(
        GLOB COMMANDS
        ${CMAKE_CURRENT_LIST_DIR}/commands/*.cpp
)

target_sources(
        freertos_shell PRIVATE
        ${COMMANDS}
)

target_link_libraries(
        freertos_shell PRIVATE
        platform_stdio_uart2
        platform_freertos
        platform_port_lwip
        shell_main
        shell_commands_common
        shell_commands_freertos
        shell_commands_beken
        shell_commands_iram
        FreeRTOS-Kernel-Heap4
        hardware_wdt
        hardware_gpio
        hardware_efuse
)

bk_firmware(freertos_shell)
bk_firmware_iram(freertos_shell)
