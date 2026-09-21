#pragma once

// driver/phy/phy.h — the constants the archive and the PHY driver agree on.
//
// This one is not part of the archive itself: phy_trident.c has no compiled
// form in lib/, so the driver is ours. The values are still an interface — the
// archive passes them to phy_set_channel() and expects them back from
// phy_get_channel().

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Frequency band. This chip has one radio and it is 2.4 GHz.
enum {
    PHY_BAND_2G4 = 0,
    PHY_BAND_5G,
    PHY_BAND_MAX,
};

// Channel bandwidth. This radio only ever runs the first of these; the wider
// ones are named so a value read back off the wire can be recognized.
enum {
    PHY_CHNL_BW_20 = 0,
    PHY_CHNL_BW_40,
    PHY_CHNL_BW_80,
    PHY_CHNL_BW_160,
    PHY_CHNL_BW_80P80,
    PHY_CHNL_BW_OTHER,
};

// Which radar detection chain a channel request addresses, and the last
// argument of phy_set_channel(). No PHY driver reads it, but the archive does,
// and MM_SET_CHANNEL_REQ must carry PHY_SEC.
//
// This archive is the channel-context build of mm_task.c, whose
// MM_SET_CHANNEL_REQ handler skips phy_set_channel() when index is PHY_PRIM and
// still sends the confirmation: with channel contexts enabled the primary chain
// is driven through the context instead.
enum {
    PHY_PRIM = 0,
    PHY_SEC,
};

// The channel readback, packed the way the archive unpacks it.
struct phy_channel_info {
    uint32_t info1; // band | (chnl_type << 8) | (prim20_freq << 16)
    uint32_t info2; // center1_freq | (center2_freq << 16)
};

void phy_get_channel(struct phy_channel_info *info, uint8_t index);

#ifdef __cplusplus
}
#endif
