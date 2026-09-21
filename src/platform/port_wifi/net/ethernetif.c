// The bridge between lwIP and the archive's data path. Creating the interfaces
// is iface.c's job.
//
// Receiving: rx_alloc_stub() hands the archive a real lwIP pbuf, the archive
// fills it and calls data_outbound_stub(), and that ends up in wifi_net_rx()
// below. From there tcpip_input() takes ownership and returns at once — the
// stack itself runs in its own thread.
//
// Transmitting: lwIP calls low_level_output() on the tcpip thread, but every
// call into the archive has to happen on the Wi-Fi core task instead. So the
// pbuf is referenced and posted across, and tx_from_core() flattens it into one
// buffer there. That buffer stays alive until the descriptor it was given to
// comes round again, which is what hostdesc.orig_addr is for.

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <FreeRTOS.h>

#include "lwip/etharp.h"
#include "lwip/tcpip.h"
#include "netif/ethernet.h"

#include "libip/tx.h"
#include "wifi/bmsg.h"
#include "wifi/net.h"

#include "iface.h"

// #define DEBUG_NAME "wifi_net"
#include "debug.h"

#define ETH_HDR_LEN 14

// Room the archive takes for itself around the frame it is handed:
// txl_buffer_alloc() builds its own struct txl_buffer_tag and the 802.11 header
// by counting backwards from packet_addr, and asserts there is space to do so.
// Every one of the vendor's SoC configurations reserves the same 96 and 16
// (CFG_MSDU_RESV_HEAD_LEN, CFG_MSDU_RESV_TAIL_LEN); the tail is for the MIC or
// ICV that encryption appends.
#define TX_HEAD_ROOM 96
#define TX_TAIL_ROOM 16

// Frames naming a vif index no interface here carries. Global because there is
// no interface to count them on, and a sign either way that the LMAC and this
// port disagree about what exists.
static uint32_t s_rx_unknown_vif;
static uint32_t s_tx_unknown_vif;

uint32_t wifi_net_rx_unknown_vif(void) { return s_rx_unknown_vif; }
uint32_t wifi_net_tx_unknown_vif(void) { return s_tx_unknown_vif; }

// ---- Transmit --------------------------------------------------------------

// Buffers the archive is still holding. Keyed by descriptor so each can be freed
// the moment that descriptor goes idle.
#define TX_INFLIGHT 16

static struct {
    struct txdesc *td;
    void          *buffer;
} s_inflight[TX_INFLIGHT];

static volatile bool s_tx_reset;

// A MAC reset is about to wipe the descriptor ring. Called with interrupts
// disabled from deep inside the archive, so it only raises a flag; the freeing
// happens on the Wi-Fi core task in tx_reap().
void wifi_net_tx_reset(void) { s_tx_reset = true; }

// Frees whatever the archive has finished with, and everything at all after a
// reset. Runs before each transmit, on the WiFi core task.
static void tx_reap(void) {
    bool reset = s_tx_reset;
    s_tx_reset = false;

    for (int i = 0; i < TX_INFLIGHT; i++) {
        if (!s_inflight[i].buffer) continue;
        if (!reset && s_inflight[i].td->status != TXDESC_STA_IDLE) continue;
        vPortFree(s_inflight[i].buffer);
        s_inflight[i].buffer = NULL;
    }
}

// The peer an outgoing frame is addressed to. Read from the archive's own
// interface table rather than cached: a station's peer changes on a roam without
// the host being told, and an access point has no single peer at all.
static uint8_t tx_staid(const struct net_if *iface) {
    if (iface->type != VIF_STA) return INVALID_AP_IDX;
    return vif_info_tab[iface->vif].u.sta.ap_id;
}

// Runs on the Wi-Fi core task, and gives the pbuf reference away on every path.
void wifi_net_tx_from_core(struct pbuf *p, uint8_t vif_idx) {
    if (!p) return;

    struct net_if *iface = iface_from_vif(vif_idx);
    if (!iface) {
        s_tx_unknown_vif++;
        LOG_W("%s: frame for vif=%u, which no interface here carries", __func__, vif_idx);
        pbuf_free(p);
        return;
    }

    if (p->tot_len <= ETH_HDR_LEN) {
        iface->stat.tx_dropped++;
        pbuf_free(p);
        return;
    }

    tx_reap();

    int slot = -1;
    for (int i = 0; i < TX_INFLIGHT; i++) {
        if (!s_inflight[i].buffer) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        LOG_W("%s: too many frames still in flight", __func__);
        iface->stat.tx_dropped++;
        pbuf_free(p);
        return;
    }

    // AC_VI is where the vendor puts data frames, and the only ring deep enough
    // to matter: 64 descriptors against 1, 1 and 4 for the others.
    struct txdesc *td = tx_txdesc_prepare(AC_VI);
    if (td->status == TXDESC_STA_USED) {
        LOG_W("%s: the transmit ring is full", __func__);
        iface->stat.tx_dropped++;
        pbuf_free(p);
        return;
    }

    // The archive transmits from one flat buffer, so the chain is flattened here
    // with the archive's own room left around the frame. The pbuf is done with
    // once the copy is through; the buffer outlives it, until the descriptor it
    // was given to goes idle.
    uint16_t len    = p->tot_len;
    uint8_t *buffer = pvPortMalloc(TX_HEAD_ROOM + len + TX_TAIL_ROOM);
    if (!buffer) {
        iface->stat.tx_dropped++;
        pbuf_free(p);
        return;
    }

    uint8_t *frame  = buffer + TX_HEAD_ROOM;
    uint16_t copied = pbuf_copy_partial(p, frame, len, 0);
    pbuf_free(p);

    if (copied != len) {
        iface->stat.tx_dropped++;
        vPortFree(buffer);
        return;
    }

    s_inflight[slot].td     = td;
    s_inflight[slot].buffer = buffer;

    td->status = TXDESC_STA_USED;

    memcpy(&td->host.eth_dest_addr, frame, 6);
    memcpy(&td->host.eth_src_addr, frame + 6, 6);
    // Copied rather than read as a number: the archive stores the two bytes
    // exactly as they sit in the frame, and so must this.
    memcpy(&td->host.ethertype, frame + 12, 2);

    // The archive is handed the payload only; the addresses above are what it
    // rebuilds the 802.11 header from, over the top of the Ethernet one.
    // orig_addr has to be the start of the whole buffer, since that is what
    // txl_buffer_alloc() measures the headroom from.
    td->host.orig_addr   = (uint32_t)buffer;
    td->host.packet_addr = (uint32_t)(frame + ETH_HDR_LEN);
    td->host.packet_len  = (uint16_t)(len - ETH_HDR_LEN);
    // Where the hardware writes the transmit status. It lands on payload that
    // has already gone out by then, which is what the vendor does too.
    td->host.status_desc_addr = (uint32_t)(frame + ETH_HDR_LEN);

    td->host.flags   = 0;
    td->host.tid     = 0xFF; // no QoS: this port never negotiates WMM
    td->host.vif_idx = iface->vif;
    td->host.staid   = tx_staid(iface);

    td->lmac.agg_desc = NULL;

    iface->stat.tx_pushed++;
    // TODO: trace("tx", frame, len);

    txu_cntrl_push(td, AC_VI);
}

// Called by lwIP on the tcpip thread. Only takes a reference and posts it: every
// call into the archive, and the copy it needs, belongs to the core task.
static err_t low_level_output(struct netif *nif, struct pbuf *p) {
    if (p->tot_len <= ETH_HDR_LEN) return ERR_ARG;

    // netif is the first member of struct net_if, so the entry is the netif.
    struct net_if *iface = (struct net_if *)nif;

    pbuf_ref(p);
    if (bmsg_tx_sender(p, iface->vif) != 0) {
        iface->stat.tx_dropped++;
        pbuf_free(p);
        return ERR_MEM;
    }

    iface->stat.tx_posted++;
    return ERR_OK;
}

// ---- Receive ---------------------------------------------------------------

// The archive writes to whatever the allocator returns without checking it for
// NULL (rxl_cntrl.c: `hostbuf_start = (uint32_t)pbuf->payload;`), so running out
// of lwIP memory cannot be reported by returning nothing. This one buffer absorbs
// those frames instead, and is never given to lwIP.
//
// Sized for the largest frame the archive will ever copy: it drops anything from
// 2300 bytes up before the copy happens.
#define RX_SPARE_LEN 2300

static struct pbuf_custom s_spare;
static uint8_t           *s_spare_data;

static void spare_free(__unused struct pbuf *p) {}

uint32_t wifi_net_rx_alloc(void **p_ret, uint32_t len) {
    struct pbuf *p = pbuf_alloc(PBUF_RAW, (uint16_t)len, PBUF_RAM);

    if (!p && s_spare_data && len <= RX_SPARE_LEN) {
        s_spare.custom_free_function = spare_free;
        p = pbuf_alloced_custom(PBUF_RAW, (uint16_t)len, PBUF_REF, &s_spare, s_spare_data, RX_SPARE_LEN);
    }

    *p_ret = p;
    return p ? 0 : 1;
}

// Runs on the Wi-Fi core task, and gives the pbuf away on every path.
void wifi_net_rx(struct pbuf *p, uint8_t vif_idx) {
    if (!p) return;

    struct net_if *iface = iface_from_vif(vif_idx);
    if (!iface) {
        s_rx_unknown_vif++;
        LOG_W("%s: frame for vif=%u, which no interface here carries", __func__, vif_idx);
        // The spare is never given away: its memory is reused by the next frame.
        if (p != &s_spare.pbuf) pbuf_free(p);
        return;
    }

    // The spare never reaches lwIP: its memory is reused by the next frame.
    if (p == &s_spare.pbuf) {
        iface->stat.rx_dropped++;
        if (iface->stat.rx_dropped % 64 == 1) {
            LOG_W("out of lwIP memory, %lu frames dropped", iface->stat.rx_dropped);
        }
        return;
    }

    if (!netif_is_link_up(&iface->netif)) {
        pbuf_free(p);
        return;
    }

    if (p->len >= ETH_HDR_LEN) {
        ++iface->stat.rx_frames;
        // TODO: trace RX (const uint8_t *)p->payload, p->tot_len
    }

    // tcpip_input() only posts to the stack's mailbox, so this returns long
    // before the frame is looked at.
    if (tcpip_input(p, &iface->netif) != ERR_OK) pbuf_free(p);
}

// ---- Interface -------------------------------------------------------------

err_t ethernetif_init(struct netif *netif) {
    // Out of the FreeRTOS heap: it must not come from the lwIP heap it exists to
    // survive the exhaustion of.
    if (!s_spare_data) {
        s_spare_data = pvPortMalloc(RX_SPARE_LEN);
        if (!s_spare_data) LOG_W("%s: no spare receive buffer; a full lwIP heap will now fault", __func__);
    }

    netif->name[0] = 'w';
    netif->name[1] = 'l';

    netif->output     = etharp_output;
    netif->linkoutput = low_level_output;

    netif->mtu   = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET | NETIF_FLAG_IGMP;

    return ERR_OK;
}
