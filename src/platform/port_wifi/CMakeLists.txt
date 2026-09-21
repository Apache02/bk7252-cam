# The RivieraWaves LMAC/UMAC, taken from a firmware known to run WiFi on this
# exact board — "the reference build" wherever the port cites it. Every symbol
# it imports and does not define is in the sources below.
add_library(libip_7221u STATIC IMPORTED GLOBAL)
set_target_properties(libip_7221u PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/libip_7221u.a)

# include/wifi/ is the surface an application may use; include/libip/ mirrors
# the archive's own directory layout, one header per subsystem it exposes.
#
# Sources are grouped by role and no include crosses a group — a header two
# groups need is published under include/wifi/ instead, which is why nothing but
# include/ is on the path below.
add_library(port_wifi INTERFACE)

target_include_directories(
        port_wifi INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_sources(port_wifi INTERFACE
    api/connect.cpp
    api/scan.cpp

    # The message path to the LMAC and the tasks it runs on.
    core/bmsg.c
    core/rw_msg.c
    core/rwnx_intf.c
    core/wifi_core.c

    net/ethernetif.c
    net/iface.c

    # The only sources here that drive hardware registers.
    radio/bk7221u_cal.c
    radio/phy_trident.c

    # Symbols the archive imports from us.
    glue/globals.c
    glue/mac_glue.c
    glue/misc.c
    glue/os_mem.c
    glue/platform_glue.c
    glue/power_save.c
    glue/wifi_mgmt.c
)

# platform_stdio_* is deliberately absent: nearly every source includes debug.h
# from it, and naming one here would pick the UART for every application. The
# application links the variant it wants, and this library will not build
# without one.
target_link_libraries(port_wifi INTERFACE
    libip_7221u
    platform_cpu
    platform_soc # rwnx/*.h and platform/soc.h, used directly by radio/phy_trident.c
    platform_freertos
    platform_freertos_beken
    platform_promise
    platform_port_lwip
    utils_busy_wait
    hardware_sctrl
    hardware_intc
)

add_subdirectory(shell_commands)
