function(simple_drivers_library NAME)
    add_library(${NAME} INTERFACE)
    target_include_directories(${NAME} INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
endfunction()

add_subdirectory(hardware_gpio)
add_subdirectory(hardware_intc)
add_subdirectory(common)

add_library(
        platform_drivers INTERFACE
)

target_include_directories(
        platform_drivers INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(
        platform_drivers INTERFACE
        drivers_common
)

target_sources(
        platform_drivers INTERFACE
        wdt.c
        uart.c
        time.c
)

