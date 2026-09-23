add_executable(
        test_i2c1_scan
        test_i2c1_scan.cpp
)

target_link_libraries(
        test_i2c1_scan PRIVATE
        platform_nosys
        platform_stdio_uart2
        platform_cpu
        hardware_wdt
        hardware_icu
        hardware_gpio
        hardware_jpeg
        hardware_sctrl
        hardware_i2c
        utils_busy_wait
)

bk_firmware_iram(test_i2c1_scan)
