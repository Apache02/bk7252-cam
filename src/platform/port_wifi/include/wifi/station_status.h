#pragma once

// The station's connection state, as the vendor binaries understand it.
//
// The archive reads it back through mhdr_get_station_status() and compares
// against MSG_CONN_SUCCESS and MSG_GOT_IP (me_task.o, ps.o, td.o), so the
// numbering has to be its own. These values come from libip_7221u.a's DWARF
// (enum msg_sta_states).
//
// Nothing sets it on our behalf except the wrong-password case: the host is
// what has to handle SM_CONNECT_IND and say so. See connect.cpp.

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MSG_IDLE         = 0,
    MSG_CONNECTING   = 1,
    MSG_PASSWD_WRONG = 2,
    MSG_NO_AP_FOUND  = 3,
    MSG_CONN_FAIL    = 4,
    MSG_CONN_SUCCESS = 5,
    MSG_GOT_IP       = 6,
} msg_sta_states;

// imported: libip(me_task.o, ps.o, td.o)
msg_sta_states mhdr_get_station_status(void);

// imported: libip(sm.o) — the archive calls this itself with MSG_PASSWD_WRONG
void mhdr_set_station_status(msg_sta_states val);

#ifdef __cplusplus
}
#endif
