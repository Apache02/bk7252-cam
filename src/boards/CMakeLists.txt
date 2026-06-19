add_library(board_config INTERFACE)

if(NOT DEFINED BOARD AND DEFINED ENV{BOARD})
    set(BOARD "$ENV{BOARD}")
    message("Using BOARD from environment '${BOARD}'")
endif()

if(NOT BOARD)
    set(BOARD A9_B_V1_3)
endif()
set(BOARD ${BOARD} CACHE STRING "Target board")

if(NOT IS_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/${BOARD})
    message(FATAL_ERROR "Unknown board '${BOARD}'. Create src/boards/${BOARD}/board.h")
endif()

file(STRINGS ${CMAKE_CURRENT_LIST_DIR}/${BOARD}/board.h _soc_line REGEX "BOARD_SOC_CHIP")
string(REGEX REPLACE ".*BOARD_SOC_CHIP[ \t]+([^ \t\r\n]+).*" "\\1" SOC_CHIP_ID "${_soc_line}")

if(NOT SOC_CHIP_ID)
    message(FATAL_ERROR "board.h for '${BOARD}' is missing BOARD_SOC_CHIP definition")
endif()

set(SOC_CHIP_ID ${SOC_CHIP_ID} PARENT_SCOPE)

target_include_directories(board_config INTERFACE ${CMAKE_CURRENT_LIST_DIR}/${BOARD})
target_compile_definitions(board_config INTERFACE BOARD_${BOARD})
