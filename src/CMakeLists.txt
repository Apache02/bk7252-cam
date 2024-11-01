# libs
add_subdirectory(platform)
add_subdirectory(utils)
add_subdirectory(utils/shell)

set(SRC_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

function(bk_firmware TARGET)
    target_link_libraries(
            ${TARGET} PRIVATE
            platform_boot
    )

    target_link_options(
            ${TARGET} PUBLIC
            "LINKER:--script=${SRC_ROOT_DIR}/linker.lds"
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

    add_dependencies(
            ${TARGET}
            encrypt_crc
    )
endfunction()

function(platform_set_stdio_uart PORT)
    if ("${PORT}" STREQUAL "1")
        target_compile_definitions(platform_stdio PRIVATE PLATFORM_STDIO_UART1_ENABLED)
        target_link_libraries(platform_stdio PRIVATE hardware_uart)
    elseif ("${PORT}" STREQUAL "2")
        target_compile_definitions(platform_stdio PRIVATE PLATFORM_STDIO_UART2_ENABLED)
        target_link_libraries(platform_stdio PRIVATE hardware_uart)
    else()
        message(FATAL_ERROR "Invalid UART number")
    endif ()
endfunction()

# applications
add_subdirectory(applications)
