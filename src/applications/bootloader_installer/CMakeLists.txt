set(BOOTLOADER_CRC_BIN "" CACHE FILEPATH
    "Path to bootloader_crc.bin to embed (default: built from src/applications/bootloader).")

if(BOOTLOADER_CRC_BIN STREQUAL "")
    set(BOOTLOADER_CRC_BIN "${CMAKE_BINARY_DIR}/bootloader_crc.bin")
    set(BOOTLOADER_CRC_BIN_IS_DEFAULT TRUE)
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

# This flash-linked target only exists as a property source for
# bk_firmware_iram() below - it writes to its own flash address (0x0) and is
# never meant to be built/run on its own, only bootloader_installer--iram is.
set_target_properties(bootloader_installer PROPERTIES EXCLUDE_FROM_ALL TRUE)

# .incbin reads BOOTLOADER_CRC_BIN at assemble time, so bootloader_payload.S
# must be reassembled whenever that file changes.
set_source_files_properties(
    ${CMAKE_CURRENT_BINARY_DIR}/bootloader_payload.S
    PROPERTIES OBJECT_DEPENDS ${BOOTLOADER_CRC_BIN}
)

target_link_libraries(bootloader_installer PRIVATE
    platform_nosys
    platform_stdio_uart2
    hardware_flash
    hardware_wdt
    utils_busy_wait
)

bk_firmware_iram(bootloader_installer)

# Only when embedding our own build's output: an explicit -DBOOTLOADER_CRC_BIN=...
# override means the caller supplies that file themselves. bk_firmware_iram()
# above is what creates the bootloader_installer--iram target.
if(BOOTLOADER_CRC_BIN_IS_DEFAULT)
    add_dependencies(bootloader_installer bootloader)
    add_dependencies(bootloader_installer--iram bootloader)
endif()
