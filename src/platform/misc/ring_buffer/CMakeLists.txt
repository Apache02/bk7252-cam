add_library(
        utils_ring_buffer INTERFACE
)

target_sources(
        utils_ring_buffer INTERFACE
        ring_buffer.c
)

target_include_directories(
        utils_ring_buffer INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)
