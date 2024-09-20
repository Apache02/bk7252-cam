cmake_minimum_required(VERSION 3.19)
project(bk7252_cam)

set(CMAKE_CXX_STANDARD 17)

add_executable(
        bk7252_cam
        src/main.cpp
)

add_subdirectory(tools/encrypt_crc)