// PHY driver for the Trident radio — the port of driver/phy/phy_trident.c.
//
// That file has no compiled form in lib/, so the LMAC calls into these symbols
// expecting the vendor's behavior: everything here is a manual port of its
// register writes over our own soc/rwnx/*.h maps, checked against the reference
// build. Only the parts this port actually reached are here.

#include <stdint.h>
#include <stddef.h>

#include "platform/soc.h"
#include "hardware/sctrl.h"
#include "rwnx/agc.h"
#include "rwnx/mdm_cfg.h"
#include "rwnx/mdm_stat.h"
#include "rwnx/rc.h"

#include "libip/phy.h"
#include "wifi/platform_glue.h"

// #define DEBUG_NAME "phy_trident"
#include "debug.h"

#undef count_of
#define count_of(x) (sizeof(x) / sizeof(x[0]))

// Hardware reset values of the AGC ACI margin registers (driver/common/reg/reg_agc.h).
// Verified against this board: with nothing writing them they read back exactly these.
#define AGC_ACI20MARG0_RST 0x02040507
#define AGC_ACI20MARG1_RST 0x00000001
#define AGC_ACI20MARG2_RST 0x00000000

// 2.4 GHz TX power limits — driver/phy/phy_trident.h
#define PHY_MAX_PWR_24G 25 // dBm
#define PHY_MIN_PWR_24G (-6)
#define PHY_TX_GAIN_MIN (-35)
#define PHY_PA_GAIN_24G 29 // dBm
// gain offset: TX_GAIN_MIN + PA_GAIN_24G = -35 + 29 = -6
#define PHY_GAIN_OFT_24G (PHY_TX_GAIN_MIN + PHY_PA_GAIN_24G)

// struct phy_trd_cfg_tag — driver/phy/phy_trident.c. Overlaid on phy_cfg_tag's
// uint32_t parameters[], so the padding after path_mapping is what puts
// tx_dc_off_comp in parameters[1].
struct phy_trd_cfg {
    uint8_t  path_mapping; // MDM type (upper nibble) | MDM2RF path mapping (lower nibble)
    uint32_t tx_dc_off_comp;
};
static_assert(offsetof(struct phy_trd_cfg, tx_dc_off_comp) == 4, "");

// Marks channel state as "nothing selected yet" — the vendor's phy_init() resets
// every phy_env channel field to this, so the first phy_set_channel() always
// takes effect however plausible its arguments look.
#define PHY_UNUSED 0xFF

// Current channel state — the vendor's phy_env, written by phy_set_channel and
// read by phy_get_channel. Starts unset, not at channel 1.
static struct {
    uint8_t  band;
    uint8_t  channel_type;
    uint16_t prim20_freq;
    uint16_t center1_freq;
    uint16_t center2_freq;
} phy_state = {
    .band         = PHY_UNUSED,
    .channel_type = PHY_UNUSED,
    .prim20_freq  = PHY_UNUSED,
    .center1_freq = PHY_UNUSED,
    .center2_freq = PHY_UNUSED,
};

// bk7221u_cal.c — loads the RC and TRX register banks, under the vendor's own
// name.
extern void rwnx_cal_load_trx_rcbekn_reg_val(void);

#define FIQ_MODEM     16
#define PRI_FIQ_MODEM 10

// Registered as the modem FIQ handler by phy_mdm_init(); not imported by name.
// The real phy_mdm_isr() also runs an "unsupported modulation" diagnostic on the
// irqlsigvalid bit and ASSERT_RECs on a CCA timeout; acking is all this does.
static void phy_mdm_isr(void) { hw_mdm_cfg->irqack.v = hw_mdm_stat->irqstat.v; }

// Reset value of each of MDM_FECTRL0's three TX digital gain fields.
#define MDM_TXDIGGAINLIN0_RST 0x20

// Ports phy_trident.c's adjust_txdiggains(): backs the 20/40 MHz TX digital
// gains off in proportion to the larger of the two DC-offset compensations, so
// the DACs cannot saturate. dc_cmp = 0 leaves all three at the reset value.
static void adjust_txdiggains(uint32_t dc_cmp) {
    typeof(hw_mdm_cfg->dcoffset0) cmp = {.v = dc_cmp};

    uint32_t max_abscmp = cmp.txidcoffset0 > cmp.txqdcoffset0 ? cmp.txidcoffset0 : cmp.txqdcoffset0;
    uint32_t scaled     = MDM_TXDIGGAINLIN0_RST * (2047u - max_abscmp) / 2047u;

    hw_write_fields(hw_mdm_cfg->fectrl0,
        .tx80diggainlin0 = MDM_TXDIGGAINLIN0_RST,
        .tx40diggainlin0 = scaled,
        .tx20diggainlin0 = scaled,
    );
}

// imported: libip(mm_task.o) via phy_init() — ports phy_trident.c's
// phy_mdm_init(), the branch for chips other than BK7231N/BK7238/BK7252N, which
// is this board's bucket.
static void phy_mdm_init(uint32_t tx_dc_off_comp) {
    LOG_D("%s(tx_dc_off_comp = %08lx)", __func__, tx_dc_off_comp);

    intc_service_register(FIQ_MODEM, PRI_FIQ_MODEM, phy_mdm_isr);

    hw_write_fields(
        hw_mdm_cfg->rxtxpwrctrl,
        .rxon = 1,
        .agcccaon = 1,
        .txon = 1
    );

    hw_mdm_cfg->dcoffset0.v = tx_dc_off_comp;
    adjust_txdiggains(tx_dc_off_comp);

    hw_mdm_cfg->txctrl0.v  = 0x00000168; // 40 MHz bandwidth defaults
    hw_mdm_cfg->rxctrl0.v  = 0x00160005;
    hw_mdm_cfg->tbectrl0.v = 0x0C0F0702;

    hw_mdm_cfg->dcestimctrl.waithtstf     = 15;
    hw_mdm_cfg->dcestimctrl.delaynormalgi = 17;
    hw_mdm_cfg->stocpectrl0.cpemode       = 0;

    hw_write_fields(hw_mdm_cfg->irqctrl, .irqccatimeouten = 1);
    LOG_I("%s(): static MDM settings done", __func__);
}

// AGC microcode for this exact hardware — phy_trident.c's agc_ram_parameter[]
// from the reference build, with `#if NEW_AGC_PARA` taken (that macro is 1 there)
// and the nested `#if RF_DOWN_ENABLE` already resolved to its `#else`. The
// generic multi-chip SDK ships a different revision of this array, so the two
// are not interchangeable.
static const uint32_t s_agc_ram_parameter[512] = {
    0x30000000, 0x01000000, 0xa800000b, 0x00000000, 0x30000000, 0x91000000, 0x04000031, 0x00000000, 0x2000008f,
    0x0400005d, 0x00000000, 0x34014000, 0x10000001, 0x08000012, 0x00000000, 0x240c808f, 0x080001ac, 0x00000000,
    0x30000000, 0x41000000, 0x04000016, 0x00000000, 0x30000000, 0x51000101, 0x0400001a, 0x00000000, 0x34028000,
    0x12004803, 0x0810481e, 0x00000000, 0x30000000, 0x01000000, 0x04000022, 0x00000000, 0x34008001, 0x64000001,
    0x08000026, 0x00000000, 0x3402800f, 0x68002101, 0x0800002a, 0x00000000, 0x2000139f, 0x0412080b, 0x00000000,
    0x4400438f, 0xa410497c, 0x081049a0, 0x00000000, 0x50000000, 0x51000101, 0x1851003e, 0x0c000036, 0x00000000,
    0x30000000, 0x12002701, 0x1c00003a, 0x00000000, 0x30000000, 0x61020202, 0x04000042, 0x00000000, 0x30000000,
    0x12000601, 0x1c000042, 0x00000000, 0x90000000, 0x52000000, 0x20900049, 0x28b00049, 0x30d00049, 0x04000008,
    0x00000000, 0x24010000, 0x0800004c, 0x00000000, 0x3400400e, 0x52000000, 0x08000050, 0x00000000, 0x340204ae,
    0x31191910, 0x08000054, 0x00000000, 0x30000eae, 0x51000101, 0x04000058, 0x00000000, 0x60000eae, 0x0c00006c,
    0x1000006f, 0x14600072, 0x00000000, 0x340204af, 0x31191910, 0x08000061, 0x00000000, 0x30000eaf, 0x51000101,
    0x04000065, 0x00000000, 0x20000eaf, 0x04000068, 0x00000000, 0x4404ceaf, 0x61704875, 0x08000075, 0x00000000,
    0x240a0eae, 0x09800075, 0x00000000, 0x24050eae, 0x09800075, 0x00000000, 0x24010eae, 0x09800075, 0x00000000,
    0x80000eaf, 0x6580487b, 0x6410487f, 0x60104883, 0x04104887, 0x00000000, 0x30000eaf, 0x41000300, 0x0400008b,
    0x00000000, 0x30000eaf, 0x41000100, 0x0400008b, 0x00000000, 0x30000eaf, 0x41000100, 0x0400008b, 0x00000000,
    0x30000eaf, 0x41000000, 0x0400008b, 0x00000000, 0x40000eaf, 0xa4000093, 0x0400008f, 0x00000000, 0x30000000,
    0x14eaed03, 0x1c000093, 0x00000000, 0x60000000, 0x28b00098, 0x30d00098, 0x0400009b, 0x00000000, 0x24010000,
    0x0800009b, 0x00000000, 0x34008000, 0x10000000, 0x0800009f, 0x00000000, 0x34014005, 0x63000102, 0x080000a3,
    0x00000000, 0x30000007, 0x50000001, 0x040000a7, 0x00000000, 0x30000007, 0x32202071, 0x040000ab, 0x00000000,
    0x30000007, 0x61030303, 0x040000af, 0x00000000, 0x340202ef, 0x31202021, 0x080000b3, 0x00000000, 0x34050aef,
    0x90000000, 0x080000b7, 0x00000000, 0x30000aef, 0x12000004, 0x040000bb, 0x00000000, 0x30000aef, 0x33000001,
    0x040000bf, 0x00000000, 0x60000eaf, 0x658048c4, 0x658000c8, 0x041048cc, 0x00000000, 0x30000eaf, 0x41000301,
    0x040000d4, 0x00000000, 0x30000eaf, 0x41000101, 0x040000d4, 0x00000000, 0x30000eaf, 0x41000001, 0x040000d0,
    0x00000000, 0x30000eaf, 0x51000001, 0x040000d4, 0x00000000, 0x34190bef, 0x52000000, 0x080000d8, 0x00000000,
    0x30000aef, 0x33000101, 0x040000dc, 0x00000000, 0xe0000eaf, 0x6da048e5, 0x658048e9, 0x601048f1, 0x641048f9,
    0x681048ed, 0x6c1048f5, 0x041048fd, 0x00000000, 0x30000eaf, 0x41030002, 0x04000105, 0x00000000, 0x30000eaf,
    0x41000302, 0x04000125, 0x00000000, 0x30000eaf, 0x41010002, 0x04000105, 0x00000000, 0x30000eaf, 0x41000102,
    0x04000125, 0x00000000, 0x30000eaf, 0x41010002, 0x04000105, 0x00000000, 0x30000eaf, 0x41000102, 0x04000125,
    0x00000000, 0x30000eaf, 0x41000002, 0x04000101, 0x00000000, 0x3000028f, 0x51000001, 0x04000198, 0x00000000,
    0x5000028f, 0x51000001, 0xa0000198, 0x0400010a, 0x00000000, 0x3401428f, 0x10000001, 0x0800010e, 0x00000000,
    0x3401c000, 0x14eaed03, 0x08000112, 0x00000000, 0x34008000, 0x10000000, 0x08000116, 0x00000000, 0x24014005,
    0x08000119, 0x00000000, 0x3400528f, 0x3400289c, 0x0400011d, 0x00000000, 0x3000128f, 0x35d4509c, 0x04000121,
    0x00000000, 0x3000128f, 0x50000100, 0x04000169, 0x00000000, 0x7432028f, 0x65000003, 0x7c00012b, 0x80104961,
    0x08000165, 0x00000000, 0x3000028f, 0x41000000, 0x0400012f, 0x00000000, 0x8432028f, 0x8400013d, 0x80104961,
    0x08104935, 0x88104939, 0x00000000, 0x3000008f, 0x41000000, 0x881049a0, 0x00000000, 0x3000008f, 0x41000000,
    0x0410482d, 0x00000000, 0x34004005, 0x61020202, 0x08104941, 0x00000000, 0x6419000f, 0x80104946, 0x08000135,
    0x88104939, 0x00000000, 0x3000000f, 0x15eeea07, 0x1c00014e, 0x00000000, 0x3000000f, 0x51000001, 0x04000152,
    0x00000000, 0x3000000f, 0x10000001, 0x0400014a, 0x00000000, 0x24008000, 0x08000155, 0x00000000, 0x30000000,
    0x10000000, 0x04000159, 0x00000000, 0x34010005, 0x61030303, 0x0800015d, 0x00000000, 0x30000007, 0x50000001,
    0x04000161, 0x00000000, 0x3000008f, 0x41000000, 0x8810482d, 0x00000000, 0x3000038f, 0x01000000, 0x04000198,
    0x00000000, 0x4577138f, 0x70000171, 0x0800016d, 0x00000000, 0x3000038f, 0x51000100, 0x04000198, 0x00000000,
    0x3000138f, 0x41000000, 0x04000175, 0x00000000, 0x3000138f, 0x65000003, 0x04000179, 0x00000000, 0x2000108f,
    0x881049a0, 0x00000000, 0x3000018f, 0x66000001, 0x04000180, 0x00000000, 0x34018000, 0x10000001, 0x08104984,
    0x00000000, 0x30000000, 0x61010101, 0x04000188, 0x00000000, 0x2403c00f, 0x0800018b, 0x00000000, 0x5405000f,
    0x95000000, 0xb4000190, 0x08000194, 0x00000000, 0x3000000f, 0x52000001, 0x04000008, 0x00000000, 0x3000000f,
    0x52000000, 0x0400000b, 0x00000000, 0x3401438f, 0x10000001, 0x0810499c, 0x00000000, 0x34028000, 0x12004803,
    0x087049a4, 0x00000000, 0x34004000, 0x95000000, 0x0810480b, 0x00000000, 0x34028000, 0x80000000, 0x041049a8,
    0x00000000, 0x34028000, 0x95000000, 0x0410481e, 0x00000000, 0x3000008f, 0x14eaed03, 0x1c0001b0, 0x00000000,
    0x3401c000, 0x52000000, 0x080001b4, 0x00000000, 0x34008000, 0x10000000, 0x080001b8, 0x00000000, 0x24014005,
    0x080001bb, 0x00000000, 0x3000128f, 0x3400209c, 0x040001bf, 0x00000000, 0x3000128f, 0x35d4509c, 0x040001c3,
    0x00000000, 0x3000128f, 0x50000100, 0x040001c7, 0x00000000, 0x4577138f, 0x700001cb, 0x0800016d, 0x00000000,
    0x3000138f, 0x41000000, 0x040001cf, 0x00000000, 0x3000138f, 0x65000003, 0x040001d3, 0x00000000, 0x2000138f,
    0x881049a0, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x100c0d11,
};

// Outside soc/rwnx/agc.h's modeled register range: the microcode RAM sits at a
// fixed offset from the *shared* AGC/MDM 0x01000000 region (real vendor macro:
// PHY_AGC_UCODE_ADDR = REG_AGC_BASE_ADDR + 0xA000), not within the AGC control-
// register block our header (based at 0x01002000) covers.
#define PHY_AGC_UCODE_ADDR ((volatile uint32_t *)0x0100A000u)

static inline void memcpy_by_words(volatile uint32_t *dst, const uint32_t *src, size_t words_count) {
    for (size_t i = 0; i < words_count; i++) {
        dst[i] = src[i];
    }
}

// Ports phy_trident.c's phy_pre_agc_init(): reset the AGC FSM, gate off the AGC
// RAM clock, copy the microcode in, then restore both. Matches the vendor
// function 1:1 including the ordering.
static void phy_agc_pre_init(void) {
    hw_agc->cntl.agcfsmreset              = 1;
    hw_mdm_cfg->memclkctrl0.agcmemclkctrl = 0;

    memcpy_by_words(PHY_AGC_UCODE_ADDR, s_agc_ram_parameter, count_of(s_agc_ram_parameter));

    hw_mdm_cfg->clkgatefctrl0.agcclkforce = 1; // "the issue about reset agc" — vendor comment, unexplained

    hw_agc->cntl.agcfsmreset              = 0;
    hw_mdm_cfg->memclkctrl0.agcmemclkctrl = 1;
    LOG_I("%s(): AGC microcode loaded", __func__);
}

// imported: libip(mm_task.o) via phy_init() — ports phy_trident.c's
// phy_agc_init(). The values come from the reference build rather than the
// generic multi-chip SDK, whose per-chip bucket values drifted between releases.
static void phy_agc_init(void) {
    phy_agc_pre_init();

    hw_agc->evtsat.v           = 0x05044804;
    hw_agc->evtdet.v           = 0x3D449008;
    hw_agc->evtdis.v           = 0x3955B00B;
    hw_agc->evtdsssdet.v       = 0x04F7480F;
    hw_agc->sat.v              = 0x08393537; // ADC saturation threshold
    hw_agc->gainrg.rfgainmaxdb = 0x4b;
    hw_agc->cross.v            = 0x002803f0; // disable crossing detection
    hw_agc->ramp.v             = 0x07200710; // reduce ramp-down detection level

    // RWNXAGCCCA1: RiseThreshold=-61, FallThreshold=-65 (non-ATE default)
    hw_agc->cca1.v            = (hw_agc->cca1.v & ~0x000ff1ffu) | 0x000bf0c3u;
    hw_agc->ccactrl.v         = (hw_agc->ccactrl.v & ~0x00000fffu) | 0x00000377u;
    hw_agc->ccatimeout        = 8000000; // CCA timeout, 100 ms
    hw_agc->dsp0.vpeakadcqdbv = (uint8_t)-32;
    LOG_I("%s(): cca configured", __func__);
}

struct phy_cfg_tag {
    uint32_t parameters[16];
};

// Ports phy_trident_init() (driver/phy/phy_trident.c): RF SPI reset, load the
// TRX register bank, front-end delay, ADC. phy_trident.c is driver-level code,
// not part of any precompiled archive, so it can't be called — this is a manual
// port of its register writes over our own soc/rwnx/rc.h map.
//
// Deliberately NOT ported (confirmed empty in the real source on this board):
//   - phy_iox_init()   — empty (IO expander not present here)
//   - phy_adc8b_init() — empty ("do nothing" comment)
static void phy_trident_init(void) {
    hw_rc->cntl_stat.spi_reset = 1;
    hw_rc->cntl_stat.spi_reset = 0;

    // phy_rcbeken_init(): with CFG_SUPPORT_CALIBRATION on — our case — the long
    // list of hardcoded rc_* writes is replaced by one call that loads the RC
    // and TRX banks wholesale. See bk7221u_cal.c for what it writes.
    rwnx_cal_load_trx_rcbekn_reg_val();
    hw_rc->agc_cfg.dsel_va = 1; // TSSI/AGC gain source select (version A)

    hw_write_fields(hw_rc->fe_rx_del,
        .fe_rx_on_del = 0x12c,
    );

    // phy_adc12b_init(): the real function is `#if (TIADC_VER == 1)` an external
    // SPI-configured TI ADC, `#else` ("Beken ADC") this on-die ADDA block — no
    // board config anywhere defines TIADC_VER, so every board (this one included)
    // takes the `#else` branch.
    hw_rc->adda_reg[0] = 0x0801A554;
    hw_rc->adda_reg[1] = 0x88924204;
    hw_rc->adda_reg[2] = 0x10484806;
    hw_rc->adda_reg[3] = 0x8C0C80C8;
    hw_rc->adda_reg[4] = 0x03010000;
    hw_rc->adda_reg[5] = 0xF80022ED;
    while (hw_rc->adda_reg_stat.adda_reg_stat & 0x3fu) {
    }
}

// Ports phy_rf_init(): wait for the TRX register load to finish, set the channel
// on/off delay banks, then enable RC and the radio channel.
static void phy_rf_init(void) {
    while (hw_rc->beken_spi.trx_reg_stat != 0);

    hw_write_fields(
        hw_rc->ch0_rx_onoff_delay,
        .ch0_rx_on_delay = 1,
        .ch0_rx_off_delay = 1,
    );
    hw_write_fields(
        hw_rc->ch0_tx_onoff_delay,
        .ch0_tx_on_delay = 1,
        .ch0_tx_off_delay = 1,
    );
    hw_write_fields(
        hw_rc->ch0_pa_onoff_delay,
        .ch0_pa_on_delay = 0x10,
        .ch0_pa_off_delay = 1,
    );
    hw_write_fields(
        hw_rc->ch0_shdn_onoff_delay,
        .ch0_shdn_on_delay = 1,
        .ch0_shdn_off_delay = 1,
    );

    hw_rc->cntl_stat.ch0_en = 1;
    hw_rc->cntl_stat.rc_en  = 1;
    LOG_I("%s(): RC static regs loaded and RC enabled", __func__);
}

// imported: libip(mm_task.o)
void phy_init(const struct phy_cfg_tag *config) {
    LOG_D("%s(%p): %08lx %08lx", __func__, config, config->parameters[0], config->parameters[1]);
    // phy_cfg_tag.parameters[] reinterpreted the way phy_trident.c does it —
    // path_mapping in parameters[0], tx_dc_off_comp in parameters[1]. The LMAC
    // fills both from MM_START_REQ.
    const struct phy_trd_cfg *cfg = (const struct phy_trd_cfg *)config;

    // Channel state resets here, exactly as the vendor resets phy_env, because
    // the whole radio is about to be reprogrammed. It also makes the first
    // phy_set_channel() see a changed bandwidth and run phy_change_bw().
    phy_state.band         = PHY_UNUSED;
    phy_state.channel_type = PHY_UNUSED;
    phy_state.prim20_freq  = PHY_UNUSED;
    phy_state.center1_freq = PHY_UNUSED;
    phy_state.center2_freq = PHY_UNUSED;

    // No calibration calls here. The vendor's phy_init() has none, and neither
    // does its startup path — the whole calibration block is commented out in
    // the firmware this board is known to work on (func/func.c:60-79).

    phy_trident_init();
    phy_rf_init();
    phy_mdm_init(cfg ? cfg->tx_dc_off_comp : 0);
    phy_agc_init();
}

// imported: libip(mm_task.o)
void phy_stop(void) { hw_rc->cntl_stat.rc_en = 0; }

// Ports force2040_toggle(): commit the forced-bandwidth bits, then reset the
// modem core and rebuild the RC/TRX state from scratch. The rebuild is why the
// vendor's TRX bank still reads its pristine defaults after a scan.
static void force2040_toggle(uint8_t chantype) {
    typeof(hw_mdm_cfg->rxmodes) modes = {.v = hw_mdm_cfg->rxmodes.v};
    modes.force20                     = (chantype == PHY_CHNL_BW_20);
    modes.force40                     = (chantype == PHY_CHNL_BW_40);
    hw_mdm_cfg->rxmodes.v             = modes.v;

    for (volatile uint32_t i = 0; i < 1000; i++);

    sctrl_modem_core_reset();

    phy_trident_init();
    phy_rf_init();
}

// Ports phy_change_bw(), which the vendor runs from phy_set_channel() whenever
// the bandwidth differs from the one already programmed. phy_init() leaves
// chnl_type at PHY_UNUSED, so the first channel set after it always runs the
// 20 MHz branch, and five registers take their vendor values only from here:
// mdm.rxmodes.force20, mdm.txctrl0.txstartdelay, mdm.tbectrl0, agc.aci20marg0/1.
//
// The NX_MDM_VER == 11 branch (mdm_tbectrl2_set) is not ported: that macro is
// undefined throughout the vendor tree, so the branch never compiles in.
static void phy_change_bw(uint8_t bw) {
    switch (bw) {
        case PHY_CHNL_BW_20:
            hw_mdm_cfg->txctrl0.txstartdelay = 0x160;
            hw_mdm_cfg->tbectrl0.v           = 0x0C0F0700;
            hw_agc->aci20marg0.v             = 0;
            hw_agc->aci20marg1.v             = 0;
            hw_agc->aci20marg2.v             = 0;
            break;
        // 40 MHz needs the TRX side too — the vendor's rwnx_cal_set_40M_setting()
        // widens the LPFs and switches the ADC/DAC clocks. Not ported; nothing in
        // this firmware asks for 40 MHz, and the vendor's own BK7252 build has the
        // call commented out.
        case PHY_CHNL_BW_40:
            hw_mdm_cfg->txctrl0.txstartdelay = 0x160;
            hw_mdm_cfg->tbectrl0.v           = 0x0C0F0702;
            hw_agc->aci20marg0.v             = AGC_ACI20MARG0_RST;
            hw_agc->aci20marg1.v             = AGC_ACI20MARG1_RST;
            hw_agc->aci20marg2.v             = AGC_ACI20MARG2_RST;
            break;

        case PHY_CHNL_BW_80:
        case PHY_CHNL_BW_160:
        case PHY_CHNL_BW_80P80:
        default:
            break;
    }

    force2040_toggle(bw);
}

// imported: libip(chan.o, mm_bcn.o, mm_task.o) — ports the real
// phy_set_channel(), 20 MHz band 2.4 GHz only, in the vendor's write order.
//
// The RF PLL retune the vendor runs per hop (bk7011_cal_pll(), BK7231U only) is
// left out: it adds a blocking settle plus an unbounded lock poll to every hop,
// and it tripped abs3_timeout_reset_agc — a full MAC reset — on nearly every hop
// back to the operating channel while scanning from an associated station.
void phy_set_channel(uint8_t band, uint8_t type, uint16_t prim20_freq, uint16_t center1_freq, uint16_t center2_freq,
                     uint8_t index) {
    // No center frequency means no channel to program, and chspi below would
    // wrap under 2400 MHz.
    if (center1_freq == 0 && center2_freq == 0) {
        return;
    }

    // Same channel already programmed — nothing to retune.
    if (phy_state.band == band && phy_state.channel_type == type && phy_state.prim20_freq == prim20_freq &&
        phy_state.center1_freq == center1_freq && phy_state.center2_freq == center2_freq) {
        return;
    }

    // index is logged and nothing more: no PHY driver reads it (libip/phy.h).
    LOG_I("%s(band=%u, type=%u, prim20=%u, center1=%u, center2=%u, index=%u): tuning", __func__, band, type,
          prim20_freq, center1_freq, center2_freq, index);

    // Disable RC to turn everything off
    hw_rc->cntl_stat.rc_en = 0;

    // Fires on any bandwidth change, and on the first call after every phy_init()
    // since chnl_type starts at PHY_UNUSED. It resets the modem core and rebuilds
    // RC/TRX, so it has to happen before the channel is written below.
    if (phy_state.channel_type != type) {
        phy_change_bw(type);
    }

    phy_state.band         = band;
    phy_state.channel_type = type;
    phy_state.prim20_freq  = prim20_freq;
    phy_state.center1_freq = center1_freq;
    phy_state.center2_freq = center2_freq;

    // phy_set_band(PHY_BAND_2G4): allow DSSS in the modem, drop the AGC's
    // OFDM-only restriction.
    hw_mdm_cfg->rxmodes.rxallowdsss = 1;
    hw_agc->cntl.ofdmonly           = 0;

    hw_rc->trx.pll_ldo_chan.chspi = (uint32_t)center1_freq - 2400u;

    hw_mdm_cfg->rxmodes.psselect20 = 0; // PSEL, nonzero only for 40/80 MHz
    hw_mdm_cfg->rxmodes.psselect40 = 0;

    hw_rc->cntl_stat.rc_en = 1; // re-enable RC — triggers the retune

    hw_write_fields(hw_rc->ch0_tx_onoff_delay,
        .ch0_tx_on_delay = 0x10,
        .ch0_tx_off_delay = 0x01,
    );
    hw_write_fields(hw_rc->ch0_pa_onoff_delay,
        .ch0_pa_on_delay = 0xE0,
        .ch0_pa_off_delay = 0x0A,
    );
}

// imported: libip(bam.o, mm_task.o, ps.o, rxl_cntrl.o, rxu_cntrl.o, txl_frame.o); also bk7221u_cal.c
void phy_get_channel(struct phy_channel_info *info, __unused uint8_t index) {
    if (!info) return;
    info->info1 = (uint32_t)phy_state.band
        | ((uint32_t)phy_state.channel_type << 8)
        | ((uint32_t)phy_state.prim20_freq << 16);
    info->info2 = (uint32_t)phy_state.center1_freq
        | ((uint32_t)phy_state.center2_freq << 16);
}

// imported: libip(me_task.o) for nss, libip(hal_machw.o, rc.o, txl_buffer.o, txl_frame.o) for ntx —
// spatial streams; BK7252 is 1x1 SISO
uint8_t phy_get_nss(void) { return 0; }
uint8_t phy_get_ntx(void) { return 0; }

// imported: libip(mm_task.o)
void phy_get_version(uint32_t *version_1, uint32_t *version_2) {
    if (version_1) *version_1 = 0;
    if (version_2) *version_2 = 0;
}

// imported: libip(me_mgmtframe.o) — TX power range for rate control
void phy_get_rf_gain_capab(int8_t *max, int8_t *min) {
    if (max) *max = PHY_MAX_PWR_24G;
    if (min) *min = PHY_MIN_PWR_24G;
}

// imported: libip(tpc.o) — clamp power to 2.4 GHz range, compute gain table index
// idx = 2 * (power - gain_offset); gain_offset = TX_GAIN_MIN + PA_GAIN = -6
void phy_get_rf_gain_idx(int8_t *power, uint8_t *idx) {
    if (!power || !idx) return;
    if (*power > PHY_MAX_PWR_24G) *power = PHY_MAX_PWR_24G;
    if (*power < PHY_MIN_PWR_24G) *power = PHY_MIN_PWR_24G;
    *idx = (uint8_t)(2 * (*power - PHY_GAIN_OFT_24G));
}
