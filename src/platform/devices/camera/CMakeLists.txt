add_library(device_camera INTERFACE)

target_sources(
        device_camera INTERFACE
        camera.c
        sensors.c
        sensors/hi704.c
)

target_include_directories(
        device_camera INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(
        device_camera INTERFACE
        board_config
        hardware_gdma
        hardware_gpio
        hardware_i2c
        hardware_jpeg
        hardware_sctrl
        platform_port_newlib
)
