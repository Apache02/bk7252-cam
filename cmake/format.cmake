find_program(CLANG_FORMAT clang-format)

if(NOT CLANG_FORMAT)
    message(STATUS "clang-format not found — format/format-check targets unavailable (install: sudo apt install clang-format)")
    return()
endif()

message(STATUS "clang-format: ${CLANG_FORMAT}")

file(GLOB_RECURSE _FORMAT_FILES
    ${CMAKE_SOURCE_DIR}/src/*.c
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.h
)

add_custom_target(format
    COMMAND ${CLANG_FORMAT} --style=file -i ${_FORMAT_FILES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "clang-format: formatting sources"
    VERBATIM
)

add_custom_target(format-check
    COMMAND ${CLANG_FORMAT} --style=file --dry-run --Werror ${_FORMAT_FILES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "clang-format: checking sources (no changes written)"
    VERBATIM
)
