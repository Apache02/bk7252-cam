#pragma once

// ip/lmac/src/rwnx/rwnx.h — bringing the archive up, and the one door it has
// back out to the host.
//
// The door is a table of five function pointers the host fills in and hands
// over. Everything the archive decides not to keep for itself — a control
// message, a received frame, a request for a buffer — leaves through it. Miss
// the registration and all of that silently vanishes, with no error.
//
// Layouts and signatures are dictated by libip_7221u.a and were read out of its
// DWARF debug info. docs/wifi_rw.md has the whole picture under "Host boundary".

#include <assert.h>
#include <stdint.h>

#include "libip/ke.h"

#ifdef __cplusplus
extern "C" {
#endif

// Brings up the LMAC. The radio has to be powered before this runs, and no
// message may be sent until the WiFi core tasks exist afterwards.
void rwnxl_init(void);

// ---- Host connector (RW_CONNECTOR_T) ---------------------------------------

typedef uint32_t (*pf_msg_outbound)(struct ke_msg *msg);
typedef uint32_t (*pf_data_outbound)(void *rx_info);
typedef uint32_t (*pf_rx_alloc)(void **p_ret, uint32_t len);
typedef uint32_t (*pf_get_rx_valid_status)(void);
typedef void (*pf_tx_confirm)(void);

typedef struct {
    pf_msg_outbound        msg_outbound_func;
    pf_data_outbound       data_outbound_func;
    pf_rx_alloc            rx_alloc_func;
    pf_get_rx_valid_status get_rx_valid_status_func;
    pf_tx_confirm          tx_confirm_func;
} rw_connector_t;

// Copies the table into the archive's own g_rwnx_connector and keeps no pointer
// to the caller's copy, so a read-only table is enough.
//
// Registering is not once-and-done: the archive's reset path zeroes that global
// as a side effect, so this has to run again after every MM_RESET_REQ.
void rwnxl_register_connector(const rw_connector_t *intf);

// What data_outbound_func is handed: struct rw_rx_info_st.
// rxu_cntrl_frame_handle() fills in length and data and passes a pointer to its
// own static instance, so the frame must be consumed before returning.
struct rw_rx_info {
    uint8_t  sta_idx;
    uint8_t  vif_idx;
    uint8_t  dst_idx;
    int8_t   rssi;
    uint16_t center_freq;
    uint16_t length;
    void    *data;
};
static_assert(sizeof(struct rw_rx_info) == 12, "rxu_cntrl.o DWARF: rw_rx_info_st is 12 bytes");

// ---- Reset --------------------------------------------------------------

// The archive's own half of a MAC reset. The host's rwxl_reset_patch() hook is
// what calls it, and that hook is the only warning the host gets that the
// transmit descriptor ring is about to be zeroed.
void rwnxl_violence_reset_patch(void);

#ifdef __cplusplus
}
#endif
