add_library(
        platform_port INTERFACE
)

target_include_directories(
        platform_port INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_sources(
        platform_port INTERFACE
        port_asm.S
)
