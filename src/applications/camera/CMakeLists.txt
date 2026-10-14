add_executable(
        camera
        main.cpp
        config.cpp
        shell_handlers.cpp
        camera_web.cpp
        telnet_console.cpp
)

target_include_directories(
        camera PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/config
)

target_link_libraries(
        camera PRIVATE
        platform_stdio_uart2
        platform_freertos
        platform_port_lwip
        platform_http_server
        platform_telnet_server
        shell_main
        shell_commands_common
        shell_commands_platform
        shell_commands_freertos
        shell_commands_beken
        FreeRTOS-Kernel-Heap5
        hardware_sctrl
        hardware_uart
        hardware_wdt
        hardware_gpio
        hardware_i2c
        hardware_jpeg
        hardware_gdma
        hardware_random
        platform_flash_kv
        device_camera
        port_wifi
        shell_commands_wifi
        utils_ring_buffer
)

bk_firmware(camera)
