add_executable(
        test_flash_kv
        test_flash_kv.cpp
)

target_link_libraries(
        test_flash_kv PRIVATE
        platform_nosys
        platform_stdio
        platform_flash_kv
        hardware_flash
        hardware_wdt
        utils_busy_wait
)

bk_firmware_iram(test_flash_kv)
