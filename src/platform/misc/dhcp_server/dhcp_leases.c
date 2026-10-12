#include "dhcp_leases.h"

#include <string.h>

void dhcp_leases_clear(struct dhcp_leases *t) { memset(t, 0, sizeof(*t)); }

static void assign(struct dhcp_leases *t, int slot, const uint8_t mac[6]) {
    struct dhcp_lease *l = &t->slot[slot];
    l->used              = true;
    l->seen              = ++t->clock;
    memcpy(l->mac, mac, 6);
}

int dhcp_leases_find(struct dhcp_leases *t, const uint8_t mac[6]) {
    for (int i = 0; i < DHCP_POOL_SIZE; i++) {
        if (!t->slot[i].used || memcmp(t->slot[i].mac, mac, 6) != 0) continue;

        t->slot[i].seen = ++t->clock;
        return i;
    }
    return -1;
}

int dhcp_leases_take(struct dhcp_leases *t, const uint8_t mac[6]) {
    int slot = dhcp_leases_find(t, mac);
    if (slot >= 0) return slot;

    int oldest = 0;
    for (int i = 0; i < DHCP_POOL_SIZE; i++) {
        if (!t->slot[i].used) {
            assign(t, i, mac);
            return i;
        }
        if (t->slot[i].seen < t->slot[oldest].seen) oldest = i;
    }

    assign(t, oldest, mac);
    return oldest;
}

bool dhcp_leases_claim(struct dhcp_leases *t, int slot, const uint8_t mac[6]) {
    if (slot < 0 || slot >= DHCP_POOL_SIZE || t->slot[slot].used) return false;

    assign(t, slot, mac);
    return true;
}

void dhcp_leases_release(struct dhcp_leases *t, int slot) {
    if (slot < 0 || slot >= DHCP_POOL_SIZE) return;
    t->slot[slot].used = false;
}
