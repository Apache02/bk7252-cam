simple_drivers_library(hardware_intc)
target_link_libraries(
        hardware_intc INTERFACE
        utils
)
target_sources(
        hardware_intc INTERFACE
        intc.c
        arm_asm.S
        bk_trap.c
)

