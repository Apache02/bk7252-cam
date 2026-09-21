#pragma once

// The table of interfaces behind wifi/net.h, and the one call the two halves of
// net/ make across it: iface.c owns an entry's life, ethernetif.c reads one
// for the LMAC indices an outgoing frame has to carry.

#include <assert.h>

#include "lwip/err.h"
#include "lwip/netif.h"

#include "libip/vif.h"
#include "wifi/net.h"

#define IFACE_STA   0
#define IFACE_AP    1
#define IFACE_COUNT 2

static_assert(IFACE_COUNT <= NX_VIRT_DEV_MAX, "the LMAC keeps fewer interfaces than this table wants");

// No peer, where a station-table index is expected. The archive publishes no
// name for it; 0xFF is the index it never hands out.
#define INVALID_AP_IDX 0xFF

// iface.c — the interface the LMAC calls vif_idx, or NULL if no interface
// this port brought up carries that index.
struct net_if *iface_from_vif(uint8_t vif_idx);

// ethernetif.c — wires a fresh netif to the data path.
err_t ethernetif_init(struct netif *netif);
