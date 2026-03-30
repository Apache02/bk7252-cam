include(FetchContent)

# FreeRTOS
message("Fetching FreeRTOS-Kernel ...")
FetchContent_Declare(
        freertos_kernel
        GIT_REPOSITORY  https://github.com/FreeRTOS/FreeRTOS-Kernel.git
        GIT_TAG         V11.1.0
        GIT_SHALLOW     TRUE
)

FetchContent_GetProperties(freertos_kernel)
if (NOT freertos_kernel_POPULATED)
    FetchContent_Populate(freertos_kernel)
endif ()

set(FREERTOS_KERNEL_PATH ${freertos_kernel_SOURCE_DIR} CACHE PATH "")


# lwip
message("Fetching lwip ...")
FetchContent_Declare(
        lwip
        GIT_REPOSITORY  https://github.com/lwip-tcpip/lwip.git
        GIT_TAG         STABLE-2_2_0_RELEASE
        GIT_SHALLOW     TRUE
)
FetchContent_GetProperties(lwip)
if(NOT lwip_POPULATED)
    FetchContent_Populate(lwip)
endif()

set(LWIP_DIR ${lwip_SOURCE_DIR} CACHE PATH "")
set(LWIP_INCLUDE_DIRS
        "${LWIP_DIR}/src/include"
        "${LWIP_DIR}/contrib/ports/freertos/include"
)

