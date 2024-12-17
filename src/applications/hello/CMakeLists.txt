add_executable(
        hello
        hello.cpp
)

bk_firmware(hello)

target_link_libraries(
        hello PRIVATE
        platform_nosys
        platform_stdio_none
        hardware_uart
        utils_busy_wait
)
