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

include_directories(src/include)

target_link_options(
        ${PROJECT_NAME} PUBLIC
        "LINKER:--script=${CMAKE_CURRENT_SOURCE_DIR}/src/linker.ld"
        "LINKER:--print-memory-usage"
)

# libs
add_subdirectory(src/entry)
add_subdirectory(src/drivers)
target_link_libraries(
        ${PROJECT_NAME}
        platform_entry
        drivers
)

# Remove execute flag
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND chmod -x ${PROJECT_NAME})

#print_section_sizes(${PROJECT_NAME})
create_bin_output(${PROJECT_NAME} app)

add_custom_target(
        app_crc.bin ALL
        DEPENDS app.bin encrypt_crc
        COMMAND ${CMAKE_BINARY_DIR}/tools/encrypt_crc/encrypt_crc app.bin ${CMAKE_CURRENT_BINARY_DIR}/app_crc.bin
)
