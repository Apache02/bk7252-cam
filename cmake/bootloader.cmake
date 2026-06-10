function(bk_bootloader TARGET)
    target_link_options(
            ${TARGET} PUBLIC
            "LINKER:--script=${LINKER_DIR}/bootloader.lds"
    )

    add_custom_command(
            TARGET ${TARGET}
            PRE_BUILD
            COMMAND rm -f ${CMAKE_BINARY_DIR}/bootloader_crc.bin
    )

    file(RELATIVE_PATH RELATIVE_BINARY_DIR ${CMAKE_SOURCE_DIR} "${CMAKE_BINARY_DIR}")

    add_custom_command(
            TARGET ${TARGET}
            POST_BUILD
            COMMAND chmod -x ${TARGET}
            COMMAND ${CMAKE_OBJCOPY} -Obinary ${TARGET} ${TARGET}.bin
            COMMAND chmod -x ${TARGET}.bin
            COMMAND python3 ${TOOLS_DIR}/crc ${TARGET}.bin ${TARGET}_crc.bin
            COMMAND cp ${TARGET}_crc.bin ${CMAKE_BINARY_DIR}/bootloader_crc.bin
    )

    add_custom_command(
            TARGET ${TARGET}
            POST_BUILD
            COMMAND stat -c "Bootloader binary: [ %n - %s bytes ]" ${RELATIVE_BINARY_DIR}/bootloader_crc.bin
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endfunction()
