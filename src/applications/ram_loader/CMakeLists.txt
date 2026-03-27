add_executable(
        ram_loader
        main.cpp
        shell_handlers.cpp
)

file(
        GLOB COMMANDS
        ${CMAKE_CURRENT_LIST_DIR}/commands/*.cpp
)

target_sources(
        ram_loader PRIVATE
        ${COMMANDS}
)

target_include_directories(
        ram_loader INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/
)

target_link_libraries(
        ram_loader PRIVATE
        shell_main
        shell_commands_common
        shell_commands_beken
        shell_commands_iram
        platform_nosys
        platform_stdio_uart2
        hardware_intc
        hardware_gpio
        hardware_timer
        hardware_wdt
        utils_busy_wait
)

bk_firmware(ram_loader)
