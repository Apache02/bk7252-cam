# Linker scripts under src/linker/ use C-preprocessor directives (e.g.
# #include "platform/stacks.h") that GNU ld can't parse itself, so run them
# through the preprocessor first and hand ld the result.
function(bk_configure_linker_script TARGET SCRIPT_NAME)
    set(SOURCE ${LINKER_DIR}/${SCRIPT_NAME})
    get_filename_component(SCRIPT_STEM ${SCRIPT_NAME} NAME_WE)
    set(GENERATED ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.${SCRIPT_NAME})

    add_custom_command(
            OUTPUT ${GENERATED}
            COMMAND ${CMAKE_C_COMPILER} -E -P -x assembler-with-cpp
                    -I${SRC_ROOT_DIR}/platform/boot/include
                    -o ${GENERATED} ${SOURCE}
            DEPENDS ${SOURCE} ${SRC_ROOT_DIR}/platform/boot/include/platform/stacks.h
            COMMENT "Preprocessing ${SCRIPT_NAME} for ${TARGET}"
            VERBATIM
    )

    set(GEN_TARGET ${TARGET}_${SCRIPT_STEM}_lds)
    add_custom_target(${GEN_TARGET} DEPENDS ${GENERATED})
    add_dependencies(${TARGET} ${GEN_TARGET})

    target_link_options(${TARGET} PUBLIC "LINKER:--script=${GENERATED}")
endfunction()
