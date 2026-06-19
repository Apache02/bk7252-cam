cmake_minimum_required(VERSION 3.19)
project(bk7252_camera C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

# libs
set(FETCHCONTENT_BASE_DIR ${CMAKE_SOURCE_DIR}/libs CACHE PATH "" FORCE)
include(dependencies.cmake)

# python tools
set(TOOLS_DIR ${CMAKE_SOURCE_DIR}/tools)
set(SRC_ROOT_DIR ${CMAKE_SOURCE_DIR}/src)
set(LINKER_DIR ${SRC_ROOT_DIR}/linker)

# Init toolchain
include(cmake/toolchain.cmake)
include(cmake/firmware.cmake)
include(cmake/iram.cmake)
include(cmake/bootloader.cmake)
include(cmake/format.cmake)

add_compile_options(-fmacro-prefix-map=${CMAKE_CURRENT_LIST_DIR}=.)

add_subdirectory(src)
