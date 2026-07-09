# none
add_library(platform_stdio_none INTERFACE)
target_sources(platform_stdio_none INTERFACE stdio.c)
target_include_directories(platform_stdio_none INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)

# uart1
add_library(platform_stdio_uart1 INTERFACE)
target_sources(platform_stdio_uart1 INTERFACE stdio.c)
target_include_directories(platform_stdio_uart1 INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
target_compile_definitions(platform_stdio_uart1 INTERFACE PLATFORM_STDIO_UART1_ENABLED=1)

# uart2
add_library(platform_stdio_uart2 INTERFACE)
target_sources(platform_stdio_uart2 INTERFACE stdio.c)
target_include_directories(platform_stdio_uart2 INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
target_compile_definitions(platform_stdio_uart2 INTERFACE PLATFORM_STDIO_UART2_ENABLED=1)
