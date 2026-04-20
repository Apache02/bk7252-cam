simple_drivers_library(hardware_security)
target_sources(
        hardware_security INTERFACE
        aes.c
        sha.c
        rsa.c
)
