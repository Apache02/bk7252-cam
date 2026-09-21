#include <stdint.h>
#include <stddef.h>

#include "wifi/station_status.h"

#define DEBUG_NAME "wifi_mgmt"
#include "debug.h"

static msg_sta_states s_station_status = MSG_IDLE;

msg_sta_states mhdr_get_station_status(void) { return s_station_status; }

void mhdr_set_station_status(msg_sta_states val) { s_station_status = val; }

// ---- Monitor mode -----------------------------------------------------------
// Not supported. The RX path asks for a monitor callback on every frame and
// treats NULL as "monitor mode off".

typedef void (*monitor_cb_t)(uint8_t *data, int len, void *info);

// imported: libip(hal_machw.o, rxl_cntrl.o, rxu_cntrl.o)
monitor_cb_t bk_wlan_get_monitor_cb(void) { return NULL; }

// imported: libip(mm_task.o, rxl_cntrl.o, rxu_cntrl.o)
int bk_wlan_is_monitor_mode(void) { return 0; }

// ---- AP configuration -------------------------------------------------------

// imported: libip(mm_bcn.o)
// The beacon builder announces the AP's channel through here. Nothing in this
// port reads it back, so it is only logged.
void bk_wlan_ap_set_channel_config(uint8_t channel) {
    LOG_I("%s(channel=%u): AP channel announced", __func__, channel);
}

// ---- Security ---------------------------------------------------------------

// imported: libip(me_utils.o)
// Open networks only for now; the association path has no key handshake yet.
uint32_t bk_sta_cipher_is_open(void) { return 1; }
