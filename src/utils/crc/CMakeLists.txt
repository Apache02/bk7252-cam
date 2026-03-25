add_library(
        utils_crc INTERFACE
)

target_include_directories(
        utils_crc INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_sources(
        utils_crc INTERFACE
        crc32.c
)

