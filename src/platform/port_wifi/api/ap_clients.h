#pragma once

// Who is talking to the AP. Pure data: the caller serialises access.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AP_CLIENTS_MAX 4

// No LMAC station index yet, or none.
#define AP_NO_STA 0xFF

enum ap_client_state {
    AP_CLIENT_FREE = 0,
    AP_CLIENT_AUTHED,
    AP_CLIENT_ASSOC,
};

struct ap_client {
    uint8_t mac[6];
    uint8_t sta_idx;
    uint8_t state; // enum ap_client_state
};

struct ap_clients {
    struct ap_client slot[AP_CLIENTS_MAX];
};

void ap_clients_clear(struct ap_clients *t);

// The live entry for this MAC, or NULL.
struct ap_client *ap_clients_find(struct ap_clients *t, const uint8_t mac[6]);

// The entry for this MAC, created as AUTHED when it is new. NULL when the table
// is full.
struct ap_client *ap_clients_add(struct ap_clients *t, const uint8_t mac[6]);

void ap_clients_remove(struct ap_clients *t, struct ap_client *c);

// The association id for an entry: its slot number plus one.
uint16_t ap_clients_aid(const struct ap_clients *t, const struct ap_client *c);

#ifdef __cplusplus
}
#endif
