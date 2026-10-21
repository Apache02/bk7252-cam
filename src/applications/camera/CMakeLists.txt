add_executable(
        camera
        main.cpp
        version.c
        config.cpp
        main_task.cpp
        shell_handlers.cpp
        telnet_console.cpp
        web/camera_web.cpp
        web/config_json.cpp
)

set(FRONTEND_DIR ${CMAKE_CURRENT_LIST_DIR}/frontend)
set(APP_GZ ${FRONTEND_DIR}/dist/index.html.gz)
set(APP_C ${CMAKE_CURRENT_BINARY_DIR}/page_app.c)

file(GLOB_RECURSE FRONTEND_SOURCES CONFIGURE_DEPENDS ${FRONTEND_DIR}/src/*)

add_custom_command(
        OUTPUT ${APP_GZ}
        COMMAND npm run build
        WORKING_DIRECTORY ${FRONTEND_DIR}
        DEPENDS
        ${FRONTEND_SOURCES}
        ${FRONTEND_DIR}/index.html
        ${FRONTEND_DIR}/scripts/gzip.js
        COMMENT "Building the frontend"
)

add_custom_command(
        OUTPUT ${APP_C}
        COMMAND ${CMAKE_COMMAND}
        -DINPUT=${APP_GZ} -DOUTPUT=${APP_C} -DSYMBOL=page_app
        -P ${PROJECT_SOURCE_DIR}/cmake/embed_file.cmake
        DEPENDS ${APP_GZ} ${PROJECT_SOURCE_DIR}/cmake/embed_file.cmake
        COMMENT "Embedding ${APP_GZ}"
)

target_sources(camera PRIVATE ${APP_C})

set_source_files_properties(version.c PROPERTIES COMPILE_DEFINITIONS BK_VERSION="${BK_VERSION}")

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
        hardware_flash
        platform_flash_kv
        device_camera
        port_wifi
        shell_commands_wifi
        utils_ring_buffer
)

bk_firmware(camera)
add_custom_command(
        TARGET camera
        POST_BUILD
        COMMAND echo "Version: " ${BK_VERSION}
)
