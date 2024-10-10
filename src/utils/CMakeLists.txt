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
)

target_sources(
        utils INTERFACE
        sleep.c
        panic.c
)

