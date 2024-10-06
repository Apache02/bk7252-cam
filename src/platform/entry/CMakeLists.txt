# .S --[gcc precompiler]-> .s ---> .o
add_custom_target(
        boot_handlers_assembly ALL
        COMMAND ${CMAKE_C_COMPILER} -E ${CMAKE_CURRENT_SOURCE_DIR}/boot_handlers.S -o ${CMAKE_CURRENT_BINARY_DIR}/boot_handlers.s
        COMMAND ${CMAKE_ASM_COMPILER} ${CMAKE_CURRENT_BINARY_DIR}/boot_handlers.s -o ${CMAKE_CURRENT_BINARY_DIR}/boot_handlers_assembly.o
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/boot_handlers.S
)

add_library(
        platform_entry INTERFACE
)

target_sources(
        platform_entry INTERFACE
        boot_vectors.s
        arm.s
        arch_main.c
)

target_link_libraries(
        platform_entry INTERFACE
        ${CMAKE_CURRENT_BINARY_DIR}/boot_handlers_assembly.o
)

target_include_directories(
        platform_entry INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

add_dependencies(
        platform_entry
        boot_handlers_assembly
)