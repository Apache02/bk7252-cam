add_executable(
        test_random
        test_random.cpp
)

target_link_libraries(
        test_random PRIVATE
        platform_nosys
        platform_stdio_uart2
        utils_busy_wait
        hardware_random
)

bk_firmware_iram(test_random)
