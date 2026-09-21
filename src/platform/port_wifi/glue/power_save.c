#include <stdint.h>

// Power save is not implemented: the RF stays on, and every hook here answers
// "awake, nothing to do". Only the two the vendor binaries import are kept.

// imported: libip(hal_machw.o, me_utils.o, rwnx.o, td.o, txl_frame.o)
uint8_t power_save_if_rf_sleep(void) { return 0; }

// imported: libip(me_task.o) — releases an MCU sleep-prevention vote
void mcu_prevent_clear(uint32_t bit) { (void)bit; }
