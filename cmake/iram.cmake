function(bk_firmware_iram BASE_TARGET)
    set(RAM_TARGET "${BASE_TARGET}--iram")

    get_target_property(BASE_SOURCES ${BASE_TARGET} SOURCES)
    get_target_property(BASE_INCLUDE_DIRS ${BASE_TARGET} INCLUDE_DIRECTORIES)
    get_target_property(BASE_LINK_LIBS ${BASE_TARGET} LINK_LIBRARIES)
    get_target_property(BASE_COMPILE_DEFS ${BASE_TARGET} COMPILE_DEFINITIONS)

    add_executable(${RAM_TARGET} ${BASE_SOURCES})

    if(BASE_INCLUDE_DIRS)
        target_include_directories(${RAM_TARGET} PRIVATE ${BASE_INCLUDE_DIRS})
    endif()

    if(BASE_LINK_LIBS)
        target_link_libraries(${RAM_TARGET} PRIVATE ${BASE_LINK_LIBS})
    endif()

    if(BASE_COMPILE_DEFS)
        target_compile_definitions(${RAM_TARGET} PRIVATE ${BASE_COMPILE_DEFS})
    endif()

    bk_configure_linker_script(${RAM_TARGET} iram.lds)

    add_custom_command(
            TARGET ${RAM_TARGET}
            POST_BUILD
            COMMAND chmod -x ${RAM_TARGET}
            COMMAND ${CMAKE_OBJCOPY} -Obinary ${RAM_TARGET} ${RAM_TARGET}.bin
            COMMAND chmod -x ${RAM_TARGET}.bin
            COMMAND cp ${RAM_TARGET}.bin ${CMAKE_BINARY_DIR}/app_iram.bin
    )

    file(RELATIVE_PATH RELATIVE_BINARY_DIR ${CMAKE_SOURCE_DIR} "${CMAKE_BINARY_DIR}")

    add_custom_command(
            TARGET ${RAM_TARGET}
            POST_BUILD
            COMMAND stat -c "Firmware binary: [ %n - %s bytes ]" ${RELATIVE_BINARY_DIR}/app_iram.bin
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endfunction()
