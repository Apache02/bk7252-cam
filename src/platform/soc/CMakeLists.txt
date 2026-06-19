if (NOT SOC_CHIP_ID)
    message(FATAL_ERROR "SOC_CHIP_ID is not set. Define BOARD_SOC_CHIP in src/boards/${BOARD}/board.h")
endif ()

if (NOT IS_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/${SOC_CHIP_ID})
    message(FATAL_ERROR "Unknown SOC '${SOC_CHIP_ID}'. Create src/platform/soc/${SOC_CHIP_ID}/")
endif ()

add_library(platform_soc INTERFACE)
target_include_directories(
        platform_soc INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}/${SOC_CHIP_ID}/include
)
