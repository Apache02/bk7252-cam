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
