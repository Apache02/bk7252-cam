function(simple_drivers_library NAME)
    add_library(${NAME} INTERFACE)
    target_include_directories(${NAME} INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
endfunction()

add_subdirectory(hardware_icu)
add_subdirectory(hardware_wdt)
add_subdirectory(hardware_gpio)
add_subdirectory(hardware_uart)
add_subdirectory(hardware_intc)
add_subdirectory(hardware_sctrl)
add_subdirectory(hardware_timer)
add_subdirectory(hardware_efuse)
add_subdirectory(hardware_flash)

add_library(
        platform_drivers INTERFACE
)

target_include_directories(
        platform_drivers INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

#target_link_libraries(
#        platform_drivers INTERFACE
#)

target_sources(
        platform_drivers INTERFACE
        time.c
)

