add_executable(
        freertos_wifi
        main.cpp
        shell_handlers.cpp
        http_server.cpp
)

target_include_directories(
        freertos_wifi PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/config
)

## Per-source ICU fire counters (hardware_intc/intc.c) for the `intc` shell
## command — diagnostic only, off by default in every other app since
## hardware_intc's sources are compiled per-consumer via its INTERFACE library.
## Comment out to build this app without the counters.
#target_compile_definitions(
#        freertos_wifi PRIVATE
#        INTC_COUNT_FIRES
#)

file(
        GLOB COMMANDS
        ${CMAKE_CURRENT_LIST_DIR}/commands/*.cpp
)

target_sources(
        freertos_wifi PRIVATE
        ${COMMANDS}
)

target_link_libraries(
        freertos_wifi PRIVATE
        platform_stdio_uart2
        platform_freertos
        platform_port_lwip
        shell_main
        shell_commands_common
        shell_commands_platform
        shell_commands_freertos
        shell_commands_beken
        FreeRTOS-Kernel-Heap5
        hardware_sctrl
        hardware_uart
        hardware_wdt
        hardware_gpio
        port_wifi
        shell_commands_wifi
        utils_ring_buffer
)

bk_firmware(freertos_wifi)
bk_firmware_iram(freertos_wifi)
