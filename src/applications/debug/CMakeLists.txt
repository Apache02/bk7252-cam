add_executable(
        debug
        main.cpp
)

target_include_directories(
        debug INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/
)

add_subdirectory(console)

target_link_libraries(
        debug PRIVATE
        platform_port
        platform_stdio
        platform_drivers
        hardware_gpio
        hardware_intc
        console
        utils
)

bk_firmware(debug)
