/* probe_arm.S — ARM-mode helpers for src/tests/probe experiments.
 * Permanent fixture, always compiled and linked. Rewritten per experiment.
 * Leave as a minimal stub when the experiment does not need ARM-mode code.
 */
    .arm

    .type probe_arm_nop, %function
    .global probe_arm_nop
probe_arm_nop:
    bx lr
