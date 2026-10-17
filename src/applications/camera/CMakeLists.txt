add_executable(
        camera
        main.cpp
        config.cpp
        main_task.cpp
        shell_handlers.cpp
        telnet_console.cpp
        web/camera_web.cpp
        web/config_json.cpp
)

set(PAGE_HOME_HTML ${CMAKE_CURRENT_LIST_DIR}/frontend/dist/index.html)
set(PAGE_HOME_C ${CMAKE_CURRENT_BINARY_DIR}/page_home.c)

add_custom_command(
        OUTPUT ${PAGE_HOME_C}
        COMMAND ${CMAKE_COMMAND}
        -DINPUT=${PAGE_HOME_HTML} -DOUTPUT=${PAGE_HOME_C} -DSYMBOL=page_home
        -P ${PROJECT_SOURCE_DIR}/cmake/embed_text.cmake
        DEPENDS ${PAGE_HOME_HTML} ${PROJECT_SOURCE_DIR}/cmake/embed_text.cmake
        COMMENT "Embedding ${PAGE_HOME_HTML}"
)

target_sources(camera PRIVATE ${PAGE_HOME_C})

target_include_directories(
        camera PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include
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
