# none
add_library(platform_stdio_none INTERFACE)
target_sources(platform_stdio_none INTERFACE stdio.c)
target_include_directories(platform_stdio_none INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)

# the console UART chosen by BOARD_STDIO_UART in board.h
add_library(platform_stdio INTERFACE)
target_sources(platform_stdio INTERFACE stdio.c)
target_include_directories(platform_stdio INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
target_compile_definitions(platform_stdio INTERFACE PLATFORM_STDIO_UART_ENABLED=1)
target_link_libraries(platform_stdio INTERFACE hardware_uart utils_busy_wait board_config)
