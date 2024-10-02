function(simple_drivers_library NAME)
    add_library(${NAME} INTERFACE)
    target_include_directories(${NAME} INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
endfunction()

add_subdirectory(hardware_gpio)

add_library(
        platform_drivers INTERFACE
)

target_include_directories(
        platform_drivers INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_sources(
        platform_drivers INTERFACE
        chip_id.c
        wdt.c
        uart.c
        time.c
)

