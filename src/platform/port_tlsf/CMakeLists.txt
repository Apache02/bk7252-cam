# Basic checks
if (NOT tlsf_SOURCE_DIR)
    message(FATAL_ERROR "tlsf_SOURCE_DIR required")
endif ()

add_library(tlsf_main INTERFACE)
target_include_directories(tlsf_main INTERFACE ${tlsf_SOURCE_DIR})
target_sources(tlsf_main INTERFACE ${tlsf_SOURCE_DIR}/tlsf.c)
target_compile_definitions(tlsf_main INTERFACE "tlsf_assert=(void)")
target_compile_options(tlsf_main INTERFACE "-ffunction-sections" "-fdata-sections")


add_library(FreeRTOS-TLSF-Heap-All STATIC)
target_include_directories(
        FreeRTOS-TLSF-Heap-All PRIVATE
        ${tlsf_SOURCE_DIR}
)
target_sources(
        FreeRTOS-TLSF-Heap-All PRIVATE
        heap_init.c
        freertos_adapter.c
)
target_link_libraries(
        FreeRTOS-TLSF-Heap-All PRIVATE
        tlsf_main
        platform_freertos
        FreeRTOS-newlib-allocator-adapter
)

