#pragma once

// The network interface: what turns an association into something lwIP can send
// and receive on.
//
//     wifi_net_ensure_iface(WIFI_IFACE_STA);  // creates it the first time
//     ...
//     wifi_net_link_up(vif_idx);              // once associated
//     ...
//     wifi_net_link_down();                   // when the link is lost
//
// vif_idx comes out of a ConnectResult (wifi/connect.h). Between link-up and an
// address arriving, DHCP is running; wifi_net_get_address() reports where that
// got to.

#include <stdbool.h>
#include <stdint.h>
#include "lwip/netif.h"
#include "lwip/pbuf.h"


// The interfaces there are to ask for. Only the station is implemented; the
// access point is a name with no bring-up behind it yet.
#define WIFI_IFACE_STA "sta"
#define WIFI_IFACE_AP  "ap"

// One interface, as wifi_net_ensure_iface() hands it back.
struct net_if {
    // First on purpose: lwIP hands its callbacks the address of this member, so
    // an entry can be recovered from one.
    struct netif netif;

    const char *name;    // what an application asks for the interface by
    uint8_t     type;    // enum vif_type
    uint8_t     vif;     // the LMAC's index for it, INVALID_VIF_IDX until then
    bool        started; // the LMAC has a vif for it and lwIP has the interface
    // Frame counts, for the question that follows an address never arriving:
    // which direction is silent. Nothing counts frames off the hardware — the
    // archive's transmit confirmation names no interface.
    struct {
        uint32_t rx_frames;  // received and handed to lwIP
        uint32_t rx_dropped; // no lwIP memory to put them in
        uint32_t tx_posted;  // handed over by lwIP
        uint32_t tx_pushed;  // reached the vendor binaries
        uint32_t tx_dropped; // too short, or the transmit ring was full
    } stat;
};

#ifdef __cplusplus
extern "C" {
#endif

// One of the interfaces above, or NULL for any other name. Creates nothing and
// brings nothing up, so one that has not been through wifi_net_ensure_iface()
// comes back with started false — which is what a report should show rather
// than quietly fix.
struct net_if *wifi_net_iface(const char *name);

// Replaces one interface's hardware address, which each starts with a
// compiled-in default. Must run before the interface is brought up: that is
// where the address is handed to the LMAC and to lwIP.
//
// Returns false and keeps the current address for an unknown name, for an
// interface already up, and for an address no AP would answer — a multicast one,
// or all zeros.
bool wifi_net_set_mac(const char *name, const uint8_t mac[NETIF_MAX_HWADDR_LEN]);

// Brings one of the interfaces above up if it is not up already, and returns
// it. Its vif field is what wifi/scan.h and wifi/connect.h take. Calling this
// again costs nothing and hands back the same interface.
//
// NULL when the name is not one of the two, when wifi_core_start() has not run,
// or when a step of the bring-up failed.
struct net_if *wifi_net_ensure_iface(const char *name);

// Marks the link up and starts DHCP. The peer an outgoing frame is addressed to
// is not passed in: the archive keeps it in its own interface table and the
// transmit path reads it from there.
void wifi_net_link_up(uint8_t vif_idx);

// Marks the link down and stops DHCP. Anything still queued is dropped.
void wifi_net_link_down(void);

// The current IPv4 address, netmask and gateway in host byte order. Returns
// false while the interface has no address yet — which is also how "DHCP has
// not finished" looks.
bool wifi_net_get_address(uint32_t *addr, uint32_t *netmask, uint32_t *gateway);

// Sets a fixed address, in host byte order, and stops DHCP. Passing 0 for addr
// hands the interface back to DHCP instead.
void wifi_net_set_address(uint32_t addr, uint32_t netmask, uint32_t gateway);

// Frames naming a vif index no interface here carries, in each direction. They
// are counted apart from struct net_if's own tallies because there is no
// interface to charge them to; either one growing means the LMAC and this port
// disagree about which interfaces exist.
uint32_t wifi_net_rx_unknown_vif(void);
uint32_t wifi_net_tx_unknown_vif(void);

// Somewhere for the archive to put a frame it is about to receive. 0 on success.
// It never returns NULL while a spare buffer exists, because the archive
// dereferences the result without checking.
uint32_t wifi_net_rx_alloc(void **p_ret, uint32_t len);

// A received frame, already turned into Ethernet by the archive, and the
// interface it arrived on. Takes the pbuf on every path, including the ones that
// drop it.
void wifi_net_rx(struct pbuf *p, uint8_t vif_idx);

// A MAC reset is about to wipe the transmit descriptor ring, so nothing in it
// can be trusted afterward. Safe to call with interrupts disabled.
void wifi_net_tx_reset(void);

// One outgoing frame, posted here by bmsg_tx_sender(), and the interface it is
// to leave by. Flattening and the copy the archive needs happen here rather than
// on the tcpip thread. Takes the caller's pbuf reference on every path.
void wifi_net_tx_from_core(struct pbuf *p, uint8_t vif_idx);

#ifdef __cplusplus
}
#endif
