add_executable(
        test_flash_bypass
        test_flash_bypass.cpp
)

target_link_libraries(
        test_flash_bypass PRIVATE
        platform_nosys
        platform_stdio_uart2
        platform_cpu
        hardware_wdt
        hardware_gpio
        hardware_icu
        hardware_sctrl
        utils_busy_wait
)

bk_firmware_iram(test_flash_bypass)
