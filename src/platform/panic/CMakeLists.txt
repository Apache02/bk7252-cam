add_library(platform_panic INTERFACE)

target_sources(
        platform_panic INTERFACE
        panic.c
)

target_include_directories(
        platform_panic INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(
        platform_panic INTERFACE
        hardware_uart
        hardware_wdt
        hardware_gpio
        board_config
        utils_busy_wait
)
