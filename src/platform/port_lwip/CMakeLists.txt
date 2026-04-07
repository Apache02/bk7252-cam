include(${LWIP_DIR}/src/Filelists.cmake)

target_include_directories(
        lwipcore PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/config
)

add_library(platform_port_lwip INTERFACE)

target_include_directories(
        platform_port_lwip INTERFACE
        ${LWIP_DIR}/src/include
        ${LWIP_DIR}/contrib/ports/freertos/include
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}/config
)

target_sources(
        platform_port_lwip INTERFACE
        ${LWIP_DIR}/contrib/ports/freertos/sys_arch.c
        net.c
)


target_link_libraries(
        platform_port_lwip INTERFACE
        lwipcore
        platform_freertos
        hardware_random
)
