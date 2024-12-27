cmake_minimum_required(VERSION 3.19)
project(bk7252_camera C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Tools
add_subdirectory(tools/encrypt_crc)

# Init toolchain
include(cmake/toolchain.cmake)

include(FreeRTOS_Kernel_import.cmake)

add_subdirectory(src)
