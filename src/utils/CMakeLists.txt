add_library(
        utils INTERFACE
)

target_include_directories(
        utils INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(
        utils INTERFACE
        hardware_wdt
        hardware_uart
        hardware_gpio
        board_config
        utils_busy_wait
)

target_sources(
        utils INTERFACE
        panic.c
)

