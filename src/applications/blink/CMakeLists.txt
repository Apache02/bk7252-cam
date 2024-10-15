add_executable(
        blink
        blink.cpp
)

bk_firmware(blink)

platform_set_stdio_uart(2)

target_link_libraries(
        blink PRIVATE
        platform_nosys
        platform_stdio
        hardware_gpio
        hardware_uart
        utils
)
