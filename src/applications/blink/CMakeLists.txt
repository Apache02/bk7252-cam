add_executable(
        blink
        blink.cpp
)

bk_firmware(blink)

target_link_libraries(
        blink PRIVATE
        platform_nosys
        platform_stdio_uart2
        hardware_gpio
        hardware_uart
        utils_busy_wait
)
