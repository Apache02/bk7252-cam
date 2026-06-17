add_executable(test_probe probe.cpp probe_arm.S)
target_link_libraries(test_probe PRIVATE
    platform_nosys
    platform_stdio_uart2
    hardware_wdt
    hardware_flash
    utils_busy_wait
)
bk_firmware_iram(test_probe)
