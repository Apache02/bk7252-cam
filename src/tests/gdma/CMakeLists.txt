add_executable(
        test_gdma
        test_gdma.cpp
)

target_link_libraries(
        test_gdma PRIVATE
        platform_nosys
        platform_stdio_uart2
        hardware_gdma
        hardware_time
        hardware_wdt
)

bk_firmware_iram(test_gdma)