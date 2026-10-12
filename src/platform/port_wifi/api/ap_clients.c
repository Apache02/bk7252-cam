#include "ap_clients.h"

#include <string.h>

void ap_clients_clear(struct ap_clients *t) { memset(t, 0, sizeof(*t)); }

struct ap_client *ap_clients_find(struct ap_clients *t, const uint8_t mac[6]) {
    for (int i = 0; i < AP_CLIENTS_MAX; i++) {
        struct ap_client *c = &t->slot[i];
        if (c->state != AP_CLIENT_FREE && memcmp(c->mac, mac, 6) == 0) return c;
    }
    return NULL;
}

struct ap_client *ap_clients_add(struct ap_clients *t, const uint8_t mac[6]) {
    struct ap_client *c = ap_clients_find(t, mac);
    if (c) return c;

    for (int i = 0; i < AP_CLIENTS_MAX; i++) {
        c = &t->slot[i];
        if (c->state != AP_CLIENT_FREE) continue;

        memcpy(c->mac, mac, 6);
        c->sta_idx = AP_NO_STA;
        c->state   = AP_CLIENT_AUTHED;
        return c;
    }
    return NULL;
}

void ap_clients_remove(struct ap_clients *t, struct ap_client *c) {
    (void)t;
    memset(c, 0, sizeof(*c));
}

uint16_t ap_clients_aid(const struct ap_clients *t, const struct ap_client *c) {
    return (uint16_t)((c - t->slot) + 1);
}
