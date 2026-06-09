set(BOOTLOADER_CRC_BIN "" CACHE FILEPATH
    "Path to bootloader_crc.bin to embed (default: <build>/bootloader_crc.bin).")

if(BOOTLOADER_CRC_BIN STREQUAL "")
    set(BOOTLOADER_CRC_BIN "${CMAKE_BINARY_DIR}/bootloader_crc.bin")
endif()

configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/bootloader_payload.S.in
    ${CMAKE_CURRENT_BINARY_DIR}/bootloader_payload.S
    @ONLY
)

add_executable(bootloader_installer
    main.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/bootloader_payload.S
)

target_link_libraries(bootloader_installer PRIVATE
    platform_nosys
    platform_stdio_uart2
    hardware_flash
    hardware_wdt
    utils_busy_wait
)

bk_firmware_iram(bootloader_installer)
