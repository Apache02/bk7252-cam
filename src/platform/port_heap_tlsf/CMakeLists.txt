add_library(tlsf_main INTERFACE)
target_include_directories(tlsf_main INTERFACE ${tlsf_SOURCE_DIR})
target_sources(tlsf_main INTERFACE ${tlsf_SOURCE_DIR}/tlsf.c)
target_compile_definitions(tlsf_main INTERFACE "tlsf_assert=(void)")
target_compile_options(tlsf_main INTERFACE "-ffunction-sections" "-fdata-sections")


# NOSYS
add_library(heap_tlsf_nosys STATIC)

target_include_directories(
        heap_tlsf_nosys PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
)
target_sources(
        heap_tlsf_nosys PRIVATE
        heap_nosys.c
)
target_link_libraries(
        heap_tlsf_nosys PRIVATE
        tlsf_main
)


# FREERTOS
add_library(heap_tlsf_freertos STATIC)

target_include_directories(
        heap_tlsf_freertos PRIVATE
        ${tlsf_SOURCE_DIR}
)
target_include_directories(
        heap_tlsf_freertos PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
)
target_sources(
        heap_tlsf_freertos PRIVATE
        heap_freertos.c
)
target_link_libraries(
        heap_tlsf_freertos PRIVATE
        tlsf_main
        platform_freertos
)
