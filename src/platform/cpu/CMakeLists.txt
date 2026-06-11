add_library(platform_cpu INTERFACE)

target_sources(
        platform_cpu INTERFACE
        cpu.S
)

target_include_directories(
        platform_cpu INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)
