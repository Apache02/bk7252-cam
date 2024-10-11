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

platform_set_stdio_uart(2)

target_link_libraries(
        shell PRIVATE
        platform_port
        platform_stdio
        platform_drivers
        hardware_gpio
        hardware_intc
        hardware_timer
        utils
        utils_shell
)

bk_firmware(shell)
