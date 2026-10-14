add_library(platform_flash_kv INTERFACE)

target_sources(
        platform_flash_kv INTERFACE
        flash_kv.c
        flash_kv_command.c
)

target_include_directories(
        platform_flash_kv INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(
        platform_flash_kv INTERFACE
        hardware_flash
        utils_crc
)
