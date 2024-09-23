project(bk7252_camera C CXX ASM)
cmake_minimum_required(VERSION 3.19)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(tools/encrypt_crc)


# use arm-none-eabi toolchain
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)
set(TOOLCHAIN arm-none-eabi)
set(CMAKE_C_COMPILER ${TOOLCHAIN}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN}-g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN}-as)
set(CMAKE_OBJCOPY ${TOOLCHAIN}-objcopy)
set(CMAKE_OBJDUMP ${TOOLCHAIN}-objdump)

set(CPU_OPTIONS "-mcpu=arm968e-s -mthumb-interwork")

set(CMAKE_C_FLAGS "${CPU_OPTIONS} -mthumb -O2 -nostdlib")
set(CMAKE_CXX_FLAGS "${CPU_OPTIONS} -mthumb -O2 -nostdlib")
set(CMAKE_ASM_FLAGS "${CPU_OPTIONS}")

set(CMAKE_CROSSCOMPILING TRUE)
# end of arm-none-eabi toolchain

add_subdirectory(src/entry)

add_executable(
        ${PROJECT_NAME}
        src/main.cpp
)

target_link_libraries(
        ${PROJECT_NAME}
        platform_entry
)

target_link_options(
        ${PROJECT_NAME} PUBLIC
        "LINKER:--script=${CMAKE_CURRENT_SOURCE_DIR}/src/linker.ld"
)

set_target_properties(
        ${PROJECT_NAME} PROPERTIES
        OUTPUT_NAME firmware.elf
)

add_custom_target(
        firmware.bin ALL
        DEPENDS ${PROJECT_NAME}
        COMMAND ${CMAKE_OBJCOPY} -Obinary firmware.elf ${CMAKE_CURRENT_BINARY_DIR}/firmware.bin
)

add_custom_target(
        firmware_crc.bin ALL
        DEPENDS firmware.bin encrypt_crc
        COMMAND ${CMAKE_BINARY_DIR}/tools/encrypt_crc/encrypt_crc firmware.bin ${CMAKE_CURRENT_BINARY_DIR}/firmware_crc.bin
)
