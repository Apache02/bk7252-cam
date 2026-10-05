add_executable(
        freertos_shell
        main.cpp
        shell_task.cpp
        shell_handlers.cpp
)

# Per-source ICU fire counters (hardware_intc/intc.c) for the `intc` shell
# command — diagnostic only, off by default in every other app since
# hardware_intc's sources are compiled per-consumer via its INTERFACE library.
# Comment out to build this app without the counters.
target_compile_definitions(
        freertos_shell PRIVATE
        INTC_COUNT_FIRES
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
        shell_commands_platform
        shell_commands_freertos
        shell_commands_beken
        shell_commands_iram
        device_camera
        FreeRTOS-Kernel-Heap5
        hardware_wdt
        hardware_sctrl
        hardware_gpio
        hardware_i2c
        hardware_jpeg
        hardware_gdma
        platform_rtc
)

bk_firmware(freertos_shell)
bk_firmware_iram(freertos_shell)
