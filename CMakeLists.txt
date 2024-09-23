project(bk7252_camera C CXX ASM)
cmake_minimum_required(VERSION 3.19)

set(CMAKE_CXX_STANDARD 17)

# Tools
add_subdirectory(tools/encrypt_crc)

# Init toolchain
include(cmake/toolchain.cmake)

add_executable(
        ${PROJECT_NAME}
        src/main.cpp
)

target_link_options(
        ${PROJECT_NAME} PUBLIC
        "LINKER:--script=${CMAKE_CURRENT_SOURCE_DIR}/src/linker.ld"
)

# libs
add_subdirectory(src/entry)
target_link_libraries(
        ${PROJECT_NAME}
        platform_entry
)

# Remove execute flag
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND chmod -x ${PROJECT_NAME})

print_section_sizes(${PROJECT_NAME})
create_bin_output(${PROJECT_NAME} firmware)

add_custom_target(
        firmware_crc.bin ALL
        DEPENDS firmware.bin encrypt_crc
        COMMAND ${CMAKE_BINARY_DIR}/tools/encrypt_crc/encrypt_crc firmware.bin ${CMAKE_CURRENT_BINARY_DIR}/firmware_crc.bin
)
