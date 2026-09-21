// RWNX host↔LMAC message infrastructure.
//
// The LMAC (libip_7221u.a) communicates with the host via g_rwnx_connector, a
// struct of function pointers registered by rwnx_connector_init(), which must
// run once before the WiFi core tasks start.
//
//   msg_outbound_func   — control message for TASK_API, routed through
//                         mr_kmsg_fwd → rw_msg_rx_head → rwnx_recv_msg().
//   rx_alloc_func       — RX buffer request, answered with a real lwIP pbuf.
//   data_outbound_func  — received data frame, handed to lwIP.
//   get_rx_valid_status — RX capacity check; always 1.
//   tx_confirm_func     — a data TX finished; carries no argument, so it only
//                         counts.

#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <FreeRTOS.h>
#include "platform/cpu.h"
#include "rtos.h"

#define DEBUG_NAME "rwnx_intf"
#include "debug.h"

#include "lwip/pbuf.h"

#include "libip/message.h"
#include "libip/rwnx.h"
#include "rwnx_intf.h"
#include "wifi/net.h"
#include "wifi/handlers/connect_handler.h"
#include "wifi/handlers/scan_handler.h"

// ---- List heads -----------------------------------------------------------
struct co_list rw_msg_rx_head;
struct co_list rw_msg_tx_head;

// ---- mr_kmsg_init ---------------------------------------------------------
// Must run before rwnx_connector_init(), because registering the connector is
// what lets the LMAC start pushing onto these lists.
void mr_kmsg_init(void) {
    co_list_init(&rw_msg_rx_head);
    co_list_init(&rw_msg_tx_head);
}

// ---- mr_kmsg_fwd ----------------------------------------------------------
// Called from LMAC interrupt context to forward a received message to the
// host. Pushes to rw_msg_rx_head under interrupt lock, then wakes the kmsg task.
uint32_t mr_kmsg_fwd(struct ke_msg *msg) {
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    co_list_push_back(&rw_msg_rx_head, &msg->hdr);
    GLOBAL_INT_RESTORE();
    app_set_sema();
    return 0;
}

// ---- rwnx_recv_msg --------------------------------------------------------
// Runs in the kmsg task. Drains rw_msg_rx_head:
//   • If the id matches a pending request in rw_msg_tx_head: copy the
//     confirmation and signal the semaphore that rw_msg_send() is blocked on.
//   • Otherwise it is an indication — scan results and the channel survey are
//     handled here; anything else is logged.
void rwnx_recv_msg(void) {
    GLOBAL_INT_DECLARATION();

    while (1) {
        GLOBAL_INT_DISABLE();
        struct co_list_hdr *rx_node = co_list_pop_front(&rw_msg_rx_head);
        GLOBAL_INT_RESTORE();
        if (!rx_node) break;

        struct ke_msg *rx_msg = (struct ke_msg *)rx_node;

        GLOBAL_INT_DISABLE();
        struct co_list_hdr *tx_node = co_list_pick(&rw_msg_tx_head);
        GLOBAL_INT_RESTORE();

        int matched = 0;
        while (tx_node) {
            msg_snd_node_t *tx = (msg_snd_node_t *)tx_node;
            if (rx_msg->id == tx->reqid) {
                matched = 1;
                GLOBAL_INT_DISABLE();
                co_list_extract(&rw_msg_tx_head, tx_node);
                GLOBAL_INT_RESTORE();
                if (tx->cfm && rx_msg->param_len) {
                    // param_len is whatever the LMAC put in the message; clamp
                    // it to what the caller actually allocated.
                    uint16_t n = rx_msg->param_len;
                    if (n > tx->cfm_size) {
                        LOG_W("cfm 0x%04x: %u bytes into a %u-byte buffer, truncating", (unsigned)rx_msg->id,
                              (unsigned)n, (unsigned)tx->cfm_size);
                        n = tx->cfm_size;
                    }
                    memcpy(tx->cfm, &rx_msg->param[0], n);
                }
                rtos_set_semaphore(&tx->semaphore);
                break;
            }
            GLOBAL_INT_DISABLE();
            tx_node = co_list_next(tx_node);
            GLOBAL_INT_RESTORE();
        }

        if (!matched) {
            switch (rx_msg->id) {
                case SCANU_RESULT_IND:
                    scan_ind_add_handler(&rx_msg->param[0], rx_msg->param_len);
                    break;
                case MM_CHANNEL_SURVEY_IND:
                    scan_survey_add_handler(&rx_msg->param[0], rx_msg->param_len);
                    break;
                case SCANU_START_CFM:
                    // Only reaches here when nothing was waiting on the CFM —
                    // the scan request is sent without blocking, so this is the
                    // normal end-of-sweep signal.
                    scan_finish_handler();
                    break;
                case SM_CONNECT_IND:
                    sm_connect_ind_handler(&rx_msg->param[0], rx_msg->param_len);
                    break;
                case SM_DISCONNECT_IND:
                    sm_disconnect_ind_handler(&rx_msg->param[0], rx_msg->param_len);
                    break;
                case MM_CHANNEL_SWITCH_IND:
                case MM_CHANNEL_PRE_SWITCH_IND:
                    // Nothing to do, but they arrive twice per channel during a
                    // scan on an associated interface, too often to log.
                    break;
                default:
                    LOG_I("ind id=0x%04x param_len=%u", (unsigned)rx_msg->id, (unsigned)rx_msg->param_len);
                    break;
            }
        }

        ke_msg_free(rx_msg);
    }
}

// ---- Host connector stubs --------------------------------------------------
// rw_connector_t and struct rw_rx_info are the archive's; libip/rwnx.h has them.

// The archive asks for somewhere to put a received frame. It calls lwIP's own
// pbuf_free() on some paths, so what it gets has to be a real pbuf rather than
// something merely shaped like one.
static uint32_t rx_alloc_stub(void **p_ret, uint32_t len) { return wifi_net_rx_alloc(p_ret, len); }

// The archive has already turned the frame into Ethernet by the time it gets
// here, so it goes straight to lwIP. length is what the archive actually
// received, which can be shorter than the pbuf it asked for.
static uint32_t data_outbound_stub(void *rx_info) {
    const struct rw_rx_info *info = (const struct rw_rx_info *)rx_info;
    struct pbuf             *p    = (struct pbuf *)info->data;

    if (!p) return 0;

    if (info->length && info->length <= p->tot_len) pbuf_realloc(p, info->length);
    wifi_net_rx(p, info->vif_idx);
    return 0;
}

static uint32_t rx_valid_stub(void) {
    return 1; // always ready to accept frames
}

// rwnx_connector_init — register the connector before starting LMAC tasks.
void rwnx_connector_init(void) {
    // rwnxl_register_connector() copies the struct into its own global and keeps
    // no pointer to this one, so a read-only table in flash is enough.
    static const rw_connector_t s_connector = {
        .msg_outbound_func        = (pf_msg_outbound)mr_kmsg_fwd,
        .data_outbound_func       = data_outbound_stub,
        .rx_alloc_func            = rx_alloc_stub,
        .get_rx_valid_status_func = rx_valid_stub,
        .tx_confirm_func          = NULL,
    };
    rwnxl_register_connector(&s_connector);
    LOG_I("connector registered (msg_out=%p)", (void *)mr_kmsg_fwd);
}
