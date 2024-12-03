# Basic checks
if (NOT FREERTOS_KERNEL_PATH)
    message(FATAL_ERROR "FREERTOS_KERNEL_PATH required")
endif ()


# FreeRTOS-Kernel-Core
add_library(FreeRTOS-Kernel-Core INTERFACE)
target_sources(
        FreeRTOS-Kernel-Core INTERFACE
        ${FREERTOS_KERNEL_PATH}/croutine.c
        ${FREERTOS_KERNEL_PATH}/event_groups.c
        ${FREERTOS_KERNEL_PATH}/list.c
        ${FREERTOS_KERNEL_PATH}/queue.c
        ${FREERTOS_KERNEL_PATH}/stream_buffer.c
        ${FREERTOS_KERNEL_PATH}/tasks.c
        ${FREERTOS_KERNEL_PATH}/timers.c
)

target_include_directories(FreeRTOS-Kernel-Core INTERFACE ${FREERTOS_KERNEL_PATH}/include)

## FreeRTOS-Kernel-Static
#add_library(FreeRTOS-Kernel-Static INTERFACE)
#target_compile_definitions(
#        FreeRTOS-Kernel-Static INTERFACE
#        configSUPPORT_STATIC_ALLOCATION=1
#        configKERNEL_PROVIDED_STATIC_MEMORY=1
#)
#target_link_libraries(FreeRTOS-Kernel-Static INTERFACE platform_freertos)

# FreeRTOS other
add_library(FreeRTOS-Kernel-Heap1 INTERFACE)
target_sources(FreeRTOS-Kernel-Heap1 INTERFACE ${FREERTOS_KERNEL_PATH}/portable/MemMang/heap_1.c)
target_link_libraries(FreeRTOS-Kernel-Heap1 INTERFACE platform_freertos)

add_library(FreeRTOS-Kernel-Heap2 INTERFACE)
target_sources(FreeRTOS-Kernel-Heap2 INTERFACE ${FREERTOS_KERNEL_PATH}/portable/MemMang/heap_2.c)
target_link_libraries(FreeRTOS-Kernel-Heap2 INTERFACE platform_freertos)

add_library(FreeRTOS-Kernel-Heap3 INTERFACE)
target_sources(FreeRTOS-Kernel-Heap3 INTERFACE ${FREERTOS_KERNEL_PATH}/portable/MemMang/heap_3.c)
target_link_libraries(FreeRTOS-Kernel-Heap3 INTERFACE platform_freertos)

add_library(FreeRTOS-Kernel-Heap4 INTERFACE)
target_sources(FreeRTOS-Kernel-Heap4 INTERFACE ${FREERTOS_KERNEL_PATH}/portable/MemMang/heap_4.c)
target_link_libraries(FreeRTOS-Kernel-Heap4 INTERFACE platform_freertos)

add_library(FreeRTOS-Kernel-Heap5 INTERFACE)
target_sources(FreeRTOS-Kernel-Heap5 INTERFACE ${FREERTOS_KERNEL_PATH}/portable/MemMang/heap_5.c)
target_link_libraries(FreeRTOS-Kernel-Heap5 INTERFACE platform_freertos)


# platform_freertos
add_library(platform_freertos INTERFACE)
target_sources(
        platform_freertos INTERFACE
        port.c
        port_asm.S
        entry.c
        sbrk.c
        crt.cpp
)

if (NOT FREERTOS_CONFIG_FILE_DIRECTORY)
    set(FREERTOS_CONFIG_FILE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/config)
endif ()

target_include_directories(
        platform_freertos INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${FREERTOS_CONFIG_FILE_DIRECTORY}
)
target_link_libraries(
        platform_freertos INTERFACE
        platform_boot
        platform_port_newlib
        FreeRTOS-Kernel-Core
        hardware_intc
        hardware_timer
)

