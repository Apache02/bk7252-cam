cmake_minimum_required(VERSION 3.19)
project(encrypt_crc C)

set(CMAKE_C_STANDARD 11)

add_executable(
        encrypt_crc
        encrypt.c
        abc.c
)
