add_library(board_config INTERFACE)

if(NOT DEFINED BOARD AND DEFINED ENV{BOARD})
    set(BOARD "$ENV{BOARD}")
    message("Using BOARD from environment '${BOARD}'")
endif()

set(BOARD ${BOARD} CACHE STRING "Target board configuration")
set_property(CACHE BOARD PROPERTY STRINGS "BOARD_A" "BOARD_B" "BOARD_C")

if(BOARD STREQUAL "A9_B_V1_3")
    target_compile_definitions(board_config INTERFACE BOARD_A9_B_V1_3)
else()
    message(FATAL_ERROR "Unknown or unset board type specified. Please set BOARD or ENV{BOARD} to one of: BOARD_A, BOARD_B, BOARD_C")
endif()

target_include_directories(
        board_config INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
