project(bk7252_camera C CXX ASM)
cmake_minimum_required(VERSION 3.19)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Tools
add_subdirectory(tools/encrypt_crc)

# Init toolchain
include(cmake/toolchain.cmake)

# shared include
include_directories(src/include)

# libs
add_subdirectory(src/platform)
add_subdirectory(src/utils)

function(bk_firmware TARGET)
    target_link_libraries(
            ${TARGET} PRIVATE
            platform_entry
    )

    target_link_options(
            ${TARGET} PUBLIC
            "LINKER:--script=${CMAKE_SOURCE_DIR}/src/linker.ld"
            "LINKER:--print-memory-usage"
    )

    # remove prev build
    add_custom_command(
            TARGET ${TARGET}
            PRE_BUILD
            COMMAND rm -f ${CMAKE_BINARY_DIR}/app_crc.bin
    )

    add_custom_command(
            TARGET ${TARGET}
            POST_BUILD
            COMMAND chmod -x ${TARGET}
            COMMAND ${CMAKE_OBJCOPY} -Obinary ${TARGET} ${TARGET}.bin
            COMMAND chmod -x ${TARGET}.bin
            COMMAND ${CMAKE_BINARY_DIR}/tools/encrypt_crc/encrypt_crc ${TARGET}.bin ${TARGET}_crc.bin
            COMMAND cp ${TARGET}_crc.bin ${CMAKE_BINARY_DIR}/app_crc.bin
    )

endfunction()

# applications
add_subdirectory(src/applications)
