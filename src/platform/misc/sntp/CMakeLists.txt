add_library(platform_sntp INTERFACE)

target_sources(
        platform_sntp INTERFACE
        ${LWIP_DIR}/src/apps/sntp/sntp.c
        sntp_sync.c
)

target_include_directories(
        platform_sntp INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(
        platform_sntp INTERFACE
        platform_port_lwip
        device_soft_rtc
)
