add_executable(
        test_security
        main.cpp
)

target_link_libraries(
        test_security PRIVATE
        platform_nosys
        platform_stdio_uart2
        hardware_security
        hardware_wdt
)

bk_firmware_iram(test_security)
