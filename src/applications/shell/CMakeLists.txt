add_executable(
        shell
        main.cpp
        handlers.cpp
)

file(
        GLOB COMMANDS
        ${CMAKE_CURRENT_LIST_DIR}/commands/*.cpp
)

target_sources(
        shell PRIVATE
        ${COMMANDS}
)

target_include_directories(
        shell INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/
)

target_link_libraries(
        shell PRIVATE
        shell_main
        shell_commands_common
        shell_commands_beken
        utils_busy_wait
        platform_nosys
        platform_stdio_uart2
        hardware_gpio
        hardware_intc
        hardware_timer
        hardware_sctrl
)

bk_firmware(shell)
