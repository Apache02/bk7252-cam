include(FetchContent)

# The three dependencies below are integrated manually via their source-dir
# variables. SOURCE_SUBDIR points at a directory that does not exist, so
# FetchContent_MakeAvailable() only downloads them and never calls
# add_subdirectory() on their own CMakeLists.txt.

# FreeRTOS
message("Fetching FreeRTOS-Kernel ...")
FetchContent_Declare(
        freertos_kernel
        GIT_REPOSITORY  https://github.com/FreeRTOS/FreeRTOS-Kernel.git
        GIT_TAG         V11.1.0
        GIT_SHALLOW     TRUE
        SOURCE_SUBDIR   no_cmake
)
FetchContent_MakeAvailable(freertos_kernel)

set(FREERTOS_KERNEL_PATH ${freertos_kernel_SOURCE_DIR} CACHE PATH "")


# lwip
message("Fetching lwip ...")
FetchContent_Declare(
        lwip
        GIT_REPOSITORY  https://github.com/lwip-tcpip/lwip.git
        GIT_TAG         STABLE-2_2_0_RELEASE
        GIT_SHALLOW     TRUE
        SOURCE_SUBDIR   no_cmake
)
FetchContent_MakeAvailable(lwip)

set(LWIP_DIR ${lwip_SOURCE_DIR} CACHE PATH "")
set(LWIP_INCLUDE_DIRS
        "${LWIP_DIR}/src/include"
        "${LWIP_DIR}/contrib/ports/freertos/include"
)


# tlsf
message("Fetching tlsf ...")
FetchContent_Declare(
        tlsf
        GIT_REPOSITORY  https://github.com/mattconte/tlsf.git
        GIT_TAG         deff9ab509341f264addbd3c8ada533678591905
        GIT_SHALLOW     TRUE
        SOURCE_SUBDIR   no_cmake
)
FetchContent_MakeAvailable(tlsf)
