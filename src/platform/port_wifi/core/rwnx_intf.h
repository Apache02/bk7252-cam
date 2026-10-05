#pragma once

// The host end of the LMAC message path (rwnx_intf.c), plus the one call it
// makes back into wifi_core.c. wifi_core_start() drives all of this.

#include <stdint.h>

#include "libip/co_list.h"
#include "rtos.h"

// A request waiting for its confirmation. The port's own bookkeeping; the
// archive never sees it.
//
// Shaped after MSG_SND_NODE_ST in the vendor's host-side rw_pub.h, plus
// cfm_size, which bounds a confirmation copy that would otherwise trust the
// LMAC's param_len.
typedef struct msg_snd_node {
    struct co_list_hdr hdr;
    void              *cfm;
    uint16_t           cfm_size;
    beken_semaphore_t  semaphore;
    uint16_t           reqid;
} msg_snd_node_t;

// Both live in rwnx_intf.c. rw_msg.c parks nodes on the pending-request list and
// rwnx_intf.c takes them off; the receive list is filled from the LMAC's
// interrupt and drained in the kmsg task.
extern struct co_list rw_msg_tx_head;
extern struct co_list rw_msg_rx_head;

// Initialize the pending-request and receive lists. Must run before
// rwnx_connector_init().
void mr_kmsg_init(void);

// Hand g_rwnx_connector to the LMAC. No message reaches the host before this.
void rwnx_connector_init(void);

// Drain the receive list: match confirmations to their waiting senders,
// dispatch the indications. Runs in the kmsg task.
void rwnx_recv_msg(void);

// wifi_core.c — wakes the kmsg task; called from mr_kmsg_fwd().
void app_set_sema(void);
