add_library(platform_timeout INTERFACE)

target_sources(
        platform_timeout INTERFACE
        timeout.c
)

target_include_directories(
        platform_timeout INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(
        platform_timeout INTERFACE
        hardware_timer
)
