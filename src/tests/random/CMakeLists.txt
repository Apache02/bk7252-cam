add_executable(
        random_test
        random_test.cpp
)

bk_firmware(random_test)

target_link_libraries(
        random_test PRIVATE
        platform_nosys
        platform_stdio_uart2
        utils_busy_wait
        hardware_random
)
