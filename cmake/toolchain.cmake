# Target definition
set(CMAKE_SYSTEM_NAME  Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

set(TOOLCHAIN arm-none-eabi)

# Compilers
set(CMAKE_C_COMPILER ${TOOLCHAIN}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN}-g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN}-gcc)

set(CPU_OPTIONS "-mcpu=arm968e-s -mthumb-interwork")

set(CMAKE_C_FLAGS "${CPU_OPTIONS} -mthumb -O2 --specs=nano.specs -nostartfiles -fno-exceptions")
set(CMAKE_CXX_FLAGS "${CPU_OPTIONS} -mthumb -O2 --specs=nano.specs -nostartfiles -fno-exceptions") #  -lstdc++ ??
set(CMAKE_ASM_FLAGS "${CPU_OPTIONS} -x assembler-with-cpp")

#set(CMAKE_CROSSCOMPILING TRUE)

# Binutils
set(CMAKE_OBJCOPY ${TOOLCHAIN}-objcopy)
set(CMAKE_OBJDUMP ${TOOLCHAIN}-objdump)
set(CMAKE_SIZE ${TOOLCHAIN}-size)

function(print_section_sizes TARGET)
    add_custom_command(TARGET ${TARGET} POST_BUILD COMMAND ${CMAKE_SIZE} ${TARGET})
endfunction()

function(create_bin_output TARGET OUTPUT_NAME)
    add_custom_target(${OUTPUT_NAME}.bin ALL DEPENDS ${TARGET} COMMAND ${CMAKE_OBJCOPY} -Obinary ${TARGET} ${OUTPUT_NAME}.bin)
    add_custom_command(TARGET ${OUTPUT_NAME}.bin POST_BUILD COMMAND chmod -x ${OUTPUT_NAME}.bin)
endfunction()

