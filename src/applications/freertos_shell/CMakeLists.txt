add_executable(
        freertos_shell
        main.cpp
        shell_task.cpp
        shell_handlers.cpp
)

bk_firmware(freertos_shell)

#target_include_directories(
#        freertos_shell PRIVATE
#        ${CMAKE_CURRENT_LIST_DIR}
#)

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
        utils_shell
        utils_shell_commands
        FreeRTOS-Kernel-Heap4
        hardware_wdt
        hardware_gpio
        hardware_efuse
)
