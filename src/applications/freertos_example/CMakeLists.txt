add_executable(
        freertos_example
        main.c
)

bk_firmware(freertos_example)

target_include_directories(
        freertos_example PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(
        freertos_example PRIVATE
        platform_stdio_uart2
        platform_freertos
        hardware_gpio
        utils_busy_wait
        FreeRTOS-Kernel-Heap4
)
