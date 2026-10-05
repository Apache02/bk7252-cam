#include <stdint.h>

// Globals the vendor binaries reference directly, as data rather than through a
// call.

// imported: libip(rxl_cntrl.o)
// Set while the RF sensitivity sweep runs, which makes the archive skip its own
// retuning. Nothing in this port starts that sweep, so it stays 0.
uint32_t g_rxsens_start = 0;

// imported: libip(rwnx.o)
// Pending channel hop, read by rwnx.o inside the LMAC reset handler, which calls
// rw_msg_set_channel() when it is non-zero. A monitor-mode path, so it stays 0.
int g_set_channel_postpone_num = 0;
