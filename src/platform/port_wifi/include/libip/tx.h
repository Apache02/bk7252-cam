#pragma once

// ip/lmac/src/tx/tx_swdesc.h — the host end of the data transmit path.
//
// Sending a frame is two calls: take the next descriptor of an access
// category's ring, fill in the host half, hand it back. Everything after that
// belongs to the archive.
//
// Every layout here is from tx_swdesc.o's DWARF debug info rather than from any
// SDK header: this archive carries the plain RivieraWaves hostdesc, 48 bytes,
// without the callback / msdu_node / access category fields the later Beken
// trees add. Their rw_msdu.c writes those, so it cannot be used against these
// binaries.

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "libip/co_list.h"
#include "libip/mac.h"

#ifdef __cplusplus
extern "C" {
#endif

// Access categories, in the order the archive indexes its descriptor rings —
// the four 802.11e/WMM (WiFi Multimedia) traffic priorities, VO highest, BK
// lowest. Only AC_VI has a deep ring — 64 descriptors against 1, 1 and 4 —
// and that is the queue the vendor puts data frames on.
enum {
    AC_BK = 0,
    AC_BE,
    AC_VI,
    AC_VO,
    AC_MAX,

    // Real in this archive (NX_BEACONING was 1 at build time — confirmed by
    // nx_txdesc_cnt/txl_cfm_evt_bit's compiled size, 5 uint32_t entries, not
    // 4), but this port never sends on it: no AP mode.
    AC_BCN = AC_VO + 1,
};

#define NX_TXQ_CNT (AC_MAX + 1)

#define TXDESC_STA_IDLE 0
#define TXDESC_STA_USED 1

// What the host fills in. packet_addr/packet_len describe the 802.11 payload,
// so they start after the 14-byte Ethernet header, whose addresses go into
// eth_dest_addr/eth_src_addr and ethertype instead. orig_addr is handed back
// untouched, which is what makes it the place to record the buffer to free.
struct hostdesc {
    uint32_t        orig_addr;
    uint32_t        packet_addr;
    uint16_t        packet_len;
    uint32_t        status_desc_addr;
    struct mac_addr eth_dest_addr;
    struct mac_addr eth_src_addr;
    uint16_t        ethertype;
    uint8_t         pn[8];
    uint16_t        sn;
    uint16_t        timestamp;
    uint8_t         tid;
    uint8_t         vif_idx;
    uint8_t         staid;
    uint16_t        flags;
};
static_assert(sizeof(struct hostdesc) == 48, "tx_swdesc.o DWARF: hostdesc is 48 bytes");
static_assert(offsetof(struct hostdesc, status_desc_addr) == 12, "tx_swdesc.o DWARF");
static_assert(offsetof(struct hostdesc, eth_dest_addr) == 16, "tx_swdesc.o DWARF");
static_assert(offsetof(struct hostdesc, sn) == 38, "tx_swdesc.o DWARF");
static_assert(offsetof(struct hostdesc, flags) == 46, "tx_swdesc.o DWARF");

// Owned by the archive; described only so that struct txdesc comes out the
// right size and the host can reach lmac.hw_desc.
struct umacdesc {
    uint32_t buf_control;
    uint32_t buff_offset;
    uint16_t payl_len;
    uint8_t  head_len;
    uint8_t  hdr_len_802_2;
    uint8_t  tail_len;
};
static_assert(sizeof(struct umacdesc) == 16, "tx_swdesc.o DWARF: umacdesc is 16 bytes");

struct lmacdesc {
    void *agg_desc;
    void *buffer;
    void *hw_desc;
};
static_assert(sizeof(struct lmacdesc) == 12, "tx_swdesc.o DWARF: lmacdesc is 12 bytes");

struct txdesc {
    struct co_list_hdr list_hdr;
    struct hostdesc    host;
    struct umacdesc    umac;
    struct lmacdesc    lmac;
    uint8_t            status;
};
static_assert(sizeof(struct txdesc) == 84, "tx_swdesc.o DWARF: txdesc is 84 bytes");
static_assert(offsetof(struct txdesc, host) == 4, "tx_swdesc.o DWARF");
static_assert(offsetof(struct txdesc, umac) == 52, "tx_swdesc.o DWARF");
static_assert(offsetof(struct txdesc, lmac) == 68, "tx_swdesc.o DWARF");
static_assert(offsetof(struct txdesc, status) == 80, "tx_swdesc.o DWARF");

// Hands back the next descriptor of that access category's ring. status is
// TXDESC_STA_USED while the previous frame in that slot is still in flight.
// tx_txdesc_init() zeroes the whole ring, so orig_addr starts out NULL.
struct txdesc *tx_txdesc_prepare(uint8_t access_category);

// Takes the filled descriptor and drives it all the way to the air.
void txu_cntrl_push(struct txdesc *txdesc, uint8_t access_category);

#ifdef __cplusplus
}
#endif
