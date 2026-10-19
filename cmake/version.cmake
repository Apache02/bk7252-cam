# Firmware version string. A release build passes -DBK_VERSION=...; without it the version comes
# from git, and falls back to "unknown" outside a git checkout.
set(BK_VERSION "" CACHE STRING "Firmware version string")

if(NOT BK_VERSION)
    execute_process(
            COMMAND git -C ${CMAKE_SOURCE_DIR} describe --tags --always --dirty
            OUTPUT_VARIABLE BK_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
    )
endif()

if(NOT BK_VERSION)
    set(BK_VERSION "unknown")
endif()

message(STATUS "Firmware version: ${BK_VERSION}")
