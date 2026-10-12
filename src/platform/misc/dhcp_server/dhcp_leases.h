#pragma once

// Which client holds which address of the DHCP pool. Pure data: the caller
// serializes access, and a slot's address is the pool's first plus the slot number.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DHCP_POOL_SIZE 4

struct dhcp_lease {
    bool     used;
    uint8_t  mac[6];
    uint32_t seen; // value of dhcp_leases.clock when the client last asked
};

struct dhcp_leases {
    struct dhcp_lease slot[DHCP_POOL_SIZE];
    uint32_t          clock; // counts requests, not time: only the order matters
};

void dhcp_leases_clear(struct dhcp_leases *t);

// The slot this MAC holds, or -1. Counts as the client having just asked.
int dhcp_leases_find(struct dhcp_leases *t, const uint8_t mac[6]);

// The slot this MAC holds, else a free one, else the one whose client asked
// longest ago: clients that left without releasing would otherwise fill the pool
// for good. Never fails.
int dhcp_leases_take(struct dhcp_leases *t, const uint8_t mac[6]);

// Gives one particular slot to this MAC if it is free. False when it is not.
bool dhcp_leases_claim(struct dhcp_leases *t, int slot, const uint8_t mac[6]);

void dhcp_leases_release(struct dhcp_leases *t, int slot);

#ifdef __cplusplus
}
#endif
