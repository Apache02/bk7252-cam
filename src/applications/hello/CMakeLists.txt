add_executable(
        hello
        hello.cpp
)

target_link_libraries(
        hello PRIVATE
        platform_nosys
        platform_stdio_none
        hardware_uart
        utils_busy_wait
)

bk_firmware(hello)
bk_firmware_iram(hello)
