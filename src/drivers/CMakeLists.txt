add_library(
        drivers INTERFACE
)

target_include_directories(
        drivers INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_sources(
        drivers INTERFACE
        wdt.c
        uart.c
)

