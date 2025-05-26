cmake_minimum_required(VERSION 3.19)
project(bk7252_camera C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

set(TOOLS_DIR ${CMAKE_CURRENT_LIST_DIR}/tools)

# Init toolchain
include(cmake/toolchain.cmake)
add_compile_options(-fmacro-prefix-map=${CMAKE_CURRENT_LIST_DIR}=.)

include(FreeRTOS_Kernel_import.cmake)

add_subdirectory(src)
