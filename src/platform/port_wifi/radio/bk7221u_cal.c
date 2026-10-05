// Radio register loading and transmit gain — the part of the vendor's
// calibration library this port actually needs, written out in full.
//
// rwnx_cal_load_trx_rcbekn_reg_val() loads two banks of constants and nothing
// else: 37 registers in the RC block, then the 28-register BK7011 transceiver
// bank behind the RC block's SPI shim. No calibration measures this board — the
// reference build has that whole block commented out of its func.c — so both
// banks are compile-time tables and can be written straight out.
//
// Values come from the vendor's RC_BEKEN_REGS and TRX_BEKEN_REGS tables for its
// BK7231U board, the same board configuration this one builds as. Where a dump
// off a running board disagrees with a table, the comment says so and the table
// wins: the register was rewritten later at runtime and the dump caught that,
// not the power-up value.
//
#include <stdint.h>

#include "libip/phy.h"
#include "rwnx/rc.h"
#include "utils/busy_wait.h"

// The transceiver bank cannot be read back — every write goes out over the RC
// block's SPI shim and the vendor never reads a word of it, keeping its own RAM
// copy instead. Only the registers touched after the initial load are shadowed
// here; every one of them is a read-modify-write of a few fields.
static uint32_t s_trx_reg3;  // pll_band_ctrl
static uint32_t s_trx_reg6;  // lpf_capcal_dcoc
static uint32_t s_trx_reg10; // tx_pa_bias
static uint32_t s_trx_reg11; // tx_gain_mod
static uint32_t s_trx_reg12; // tx_gain_pa
static uint32_t s_trx_reg16; // en_map2

// reg6's power-up value, the one the OFDM rates want back.
#define TRX_REG6_RESET 0x5FA44100u

// Ports the vendor's CAL_WR_TRXREGS(): the status bit for this register has to
// be clear before the write, and clear again after. The short wait in between
// is what gives the bit time to go up — without it the second check can pass
// before the transfer has even started.
static void trx_reg_write(unsigned index, uint32_t value) {
    volatile uint32_t *bank = (volatile uint32_t *)&hw_rc->trx;

    while (hw_rc->beken_spi.trx_reg_stat & (1u << index)) {
    }
    bank[index] = value;
    busy_wait_half_us(1);
    while (hw_rc->beken_spi.trx_reg_stat & (1u << index)) {
    }
}

// pll_band_ctrl (reg3)
#define TRX_SPI_TRIGGER   (1u << 10)
#define TRX_ERRDET_SPI_EN (1u << 11)
// en_map2 (reg16)
#define TRX_EN_RF_PLL (1u << 28)
#define TRX_EN_DPLL   (1u << 30)

// Re-locks the RF PLL: power the PLL and the doubler, then hand the band logic
// a rising and falling edge on spi_trigger and re-arm its error detector.
//
// Both bits in reg16 are already set by the bank load, so the first write only
// matters when something has powered the PLL down since.
static void bk7011_cal_pll(void) {
    s_trx_reg16 |= TRX_EN_RF_PLL | TRX_EN_DPLL;
    trx_reg_write(16, s_trx_reg16);

    s_trx_reg3 |= TRX_SPI_TRIGGER;
    trx_reg_write(3, s_trx_reg3);
    s_trx_reg3 &= ~TRX_SPI_TRIGGER;
    trx_reg_write(3, s_trx_reg3);
    s_trx_reg3 |= TRX_ERRDET_SPI_EN;
    trx_reg_write(3, s_trx_reg3);

    // The vendor's only settle here. It polls no lock bit — the band logic is
    // assumed done when this expires.
    busy_wait_us(10);
}

void rwnx_cal_load_trx_rcbekn_reg_val(void) {
    // ---- RC block, the ten registers the vendor writes first ---------------
    // A dump reads 0x00040009 here; bits [18:16] are the read-only rc_state,
    // reading ACTIVE. Only ch0_en and rc_en are ours to set.
    hw_rc->cntl_stat.v = 0x00000009; // R0x0

    hw_rc->beken_spi.v = 0xF0000000; // R0x1  SPI prescaler

    // A dump reads 0 — the LMAC drives output power down through this register
    // once it is running, so what it holds later says nothing about power-up.
    hw_rc->ch0_outpower.v = 0x00000030; // R0x5

    hw_rc->ch0_rx_onoff_delay.v = 0x00010001; // R0x8

    // These two a dump reads as 0x00010010 and 0x000A00E0: phy_set_channel()
    // programs its own post-tune delays over them after every hop. The power-up
    // values are the ones here.
    hw_rc->ch0_tx_onoff_delay.v = 0x000100E0; // R0xB
    hw_rc->ch0_pa_onoff_delay.v = 0x00010070; // R0xE

    hw_rc->ch0_shdn_onoff_delay.v = 0x00010001; // R0x11
    hw_rc->ch0_force.v            = 0x00010005; // R0x19
    hw_rc->misc_force.v           = 0x00000002; // R0x1C
    hw_rc->fe_rx_del.v            = 0x0000012C; // R0x1E

    // ---- RC block, the "NEW ADDED" group -----------------------------------
    // Written in the vendor's own order, which is not address order: R0x4E,
    // R0x5A and R0x5B come after R0x5C rather than in sequence.
    hw_rc->rx_avg_calc.v = 0x1002DF4B; // R0x3C
    hw_rc->rx_calib_en.v = 0x00000000; // R0x3E
    hw_rc->rx_error_rd.v = 0x03E803CB; // R0x3F
    hw_rc->rx_ty2_rd.v   = 0x00000001; // R0x40
    hw_rc->rx_dc_wr.v    = 0x00000000; // R0x41
    hw_rc->rx_error_wr.v = 0x02000041; // R0x42

    hw_rc->tx_mode_cfg.v       = 0x018B018B; // R0x4C
    hw_rc->tx_sin_cfg.v        = 0x2CC02000; // R0x4D
    hw_rc->tx_dc_comp.v        = 0x02000200; // R0x4F
    hw_rc->tx_gain_comp.v      = 0x03FF03FF; // R0x50
    hw_rc->tx_phase_ty2_comp.v = 0x02000200; // R0x51
    hw_rc->tx_other_cfg.v      = 0x44108800; // R0x52

    // 0x00025600 | (TSSI_POUT_TH << 1), and TSSI_POUT_TH is 0x6E on this board.
    // phy_trident_init() sets dsel_va in this register straight afterwards.
    hw_rc->agc_cfg.v = 0x000256DC; // R0x54

    hw_rc->rx_sinad_rd.v   = 0x00000000; // R0x55
    hw_rc->trx_spi_intlv.v = 0x80000064; // R0x5C

    hw_rc->hbf_cfg.v       = 0x00000005; // R0x4E
    hw_rc->cal_cap_i.v     = 0x00000000; // R0x5A
    hw_rc->cal_cap_q.v     = 0x00000000; // R0x5B
    hw_rc->adda_reg_stat.v = 0x00000000; // R0x6A

    for (unsigned i = 0; i < 8; i++) {
        hw_rc->pa_level_map[i].v = 0x00000000; // R0x70..R0x77
    }

    // ---- BK7011 transceiver bank -------------------------------------------
    // Bracket the writes with the SPI shim's status field, which carries one bit
    // per register and clears as each write lands. That field is 28 bits, so it
    // covers reg0 through reg27 and nothing else.
    while (hw_rc->beken_spi.trx_reg_stat != 0) {
    }

    // Confirmed against a dump taken off a running board after a scan
    // (`dump32 0x01050080 28`). Every word matched, including reg10 through
    // reg12, which come from this board's TRX_REG_0X{A,B,C}_VAL.
    //
    // reg5 was the one exception, and only in its chspi field, where the dump
    // caught whatever channel the scan had last swept to. The value below keeps
    // the table's channel; phy_set_channel() rewrites that field on the first
    // tune anyway.
    hw_rc->trx.tssi_ctrl.v        = 0x00039042;           // reg0
    hw_rc->trx.pll_cp_tx_lpf_rx.v = 0xF259347B;           // reg1
    hw_rc->trx.pll_ctrl_lpf_tx.v  = 0x4CAD2213;           // reg2
    hw_rc->trx.pll_band_ctrl.v = s_trx_reg3 = 0x7C945A75; // reg3
    hw_rc->trx.pll_div_mixer.v              = 0x13D0110F; // reg4
    hw_rc->trx.pll_ldo_chan.v               = 0x1804AA47; // reg5
    hw_rc->trx.lpf_capcal_dcoc.v = s_trx_reg6 = TRX_REG6_RESET; // reg6
    hw_rc->trx.lpf_rx_cfg.v                 = 0x000020F5; // reg7
    hw_rc->trx.rx_bias_rssi.v               = 0x072C29DE; // reg8
    hw_rc->trx.rx_fe_gain.v                 = 0x000163AF; // reg9
    hw_rc->trx.tx_pa_bias.v = s_trx_reg10 = 0x036F2075; // reg10
    hw_rc->trx.tx_gain_mod.v = s_trx_reg11 = 0x87248F37; // reg11
    hw_rc->trx.tx_gain_pa.v = s_trx_reg12 = 0x00228765; // reg12
    hw_rc->trx.en_map0.v                    = 0xDDF90339; // reg13
    hw_rc->trx.en_map1.v                    = 0xDA01BCF0; // reg14
    hw_rc->trx.sys_ctrl.v                   = 0x00018000; // reg15
    hw_rc->trx.en_map2.v = s_trx_reg16 = 0xD0000000;      // reg16
    hw_rc->trx.tx_sinad.v              = 0x00000000;      // reg17
    hw_rc->trx.adc_test_cfg.v          = 0xD0090481;      // reg18
    hw_rc->trx.pll_cp_cfg.v            = 0x7B305ECC;      // reg19
    hw_rc->trx.rx_dc_cal[0].v          = 0x827C827C;      // reg20
    hw_rc->trx.rx_dc_cal[1].v          = 0x86788678;      // reg21
    hw_rc->trx.rx_dc_cal[2].v          = 0x8C748C74;      // reg22
    hw_rc->trx.rx_dc_cal[3].v          = 0xA45F9868;      // reg23
    hw_rc->trx.rx_dc_cal[4].v          = 0xA45FA45F;      // reg24
    hw_rc->trx.rx_dc_cal[5].v          = 0xA55EA45F;      // reg25
    hw_rc->trx.rx_dc_cal[6].v          = 0xA55DA55E;      // reg26
    hw_rc->trx.rx_dc_cal[7].v          = 0xA55DA55D;      // reg27

    while (hw_rc->beken_spi.trx_reg_stat != 0) {
    }

    // No reg28 write. The vendor's loader stops at reg27, and rc_trx_reg28_set()
    // targets an address outside this block entirely.

    // The vendor's loader ends here too: a fresh bank means a fresh PLL lock.
    bk7011_cal_pll();
}


// ---- TX power, by rate -----------------------------------------------------
//
// The LMAC calls rwnx_cal_set_txpwr_by_rate() from its rate controller every
// time the rate for a station changes. Skipping it leaves the whole transmit
// gain chain at whatever the bank load put there, which is enough to associate
// and not enough to be heard on a data frame.
//
// The vendor's version looks up a per-channel gain in tables that are only ever
// filled from flash, adds a per-band distance offset, then a per-rate shift. In
// this build the tables are zeroed and every distance offset is 0, so the lookup
// collapses to the per-rate shift alone.
//
// The two gain tables and the four shift tables below were read out of
// the vendor's own objects rather than retyped from the SDK sources.

// One PWR_REGS row packed into a word. The vendor's PWRI() rows name the fields
// by the register bits they land in; the positions here were solved against
// those rows and the compiled table.
#define PWR_REGA_4_7(w)   (((w) >> 0) & 0x0Fu)
#define PWR_REGA_8_13(w)  (((w) >> 4) & 0x3Fu)
#define PWR_REGB_28_31(w) (((w) >> 10) & 0x0Fu)
#define PWR_REGC_0_3(w)   (((w) >> 14) & 0x0Fu)
#define PWR_REGC_4_7(w)   (((w) >> 18) & 0x0Fu)
#define PWR_REGC_8_11(w)  (((w) >> 22) & 0x0Fu)
#define PWR_PREGAIN(w)    (((w) >> 26) & 0x0Fu)

static const uint32_t cfg_tab_b[32] = {
    0x148CE9E4, 0x048CE9E4, 0x389129E4, 0x289129E4, 0x189129E4, 0x3D1129E4, 0x291129E4, 0x151129E4,
    0x091129E4, 0x395529E4, 0x315529E4, 0x295529E4, 0x215529E4, 0x195529E4, 0x115529E4, 0x299529E4,
    0x219529E4, 0x199529E4, 0x119529E4, 0x099529E4, 0x019529E4, 0x219569E4, 0x199569E4, 0x119569E4,
    0x099569E4, 0x29D569E4, 0x21D569E4, 0x19D569E4, 0x11D561E4, 0x09D561E4, 0x01D561E4, 0x01D961E4,
};

static const uint32_t cfg_tab_g[32] = {
    0x05113207, 0x29553207, 0x19553207, 0x11553207, 0x09553207, 0x21557207, 0x19557207, 0x11557207,
    0x29957207, 0x21957207, 0x19957207, 0x11957207, 0x09957207, 0x01957207, 0x19D57207, 0x11D57207,
    0x09D57207, 0x01D57207, 0x11D97207, 0x09D97207, 0x01D97207, 0x21D96207, 0x19D96207, 0x11D96207,
    0x09D96207, 0x01D96295, 0x21D9A295, 0x19D9A295, 0x11D9A295, 0x09D9A295, 0x01D9A295, 0x01D99295,
};

// Indexed from the top rate down, which is how the vendor reads them.
static const uint16_t shift_tab_b[4]   = {0, 0, 0, 0};
static const uint16_t shift_tab_g[8]   = {0, 1, 1, 2, 4, 4, 4, 4};
static const uint16_t shift_tab_n20[8] = {0, 1, 1, 2, 4, 6, 6, 6};
static const uint16_t shift_tab_n40[8] = {0, 1, 1, 2, 4, 6, 6, 6};

// Ports rwnx_cal_set_txpwr(): one gain-table row into the four places that
// carry transmit gain. dcor_mod would come from the per-channel modulator
// table, which calibration never fills, so it stays 0 like the vendor's.
static void set_txpwr(const uint32_t *table, unsigned index) {
    uint32_t row = table[index];

    hw_rc->tx_other_cfg.tx_pre_gain = PWR_PREGAIN(row);

    s_trx_reg10 = (s_trx_reg10 & ~0x3FF0u) | (PWR_REGA_4_7(row) << 4) | (PWR_REGA_8_13(row) << 8);
    trx_reg_write(10, s_trx_reg10);

    s_trx_reg11 = (s_trx_reg11 & ~0xF000F000u) | (PWR_REGB_28_31(row) << 28);
    trx_reg_write(11, s_trx_reg11);

    s_trx_reg12 = (s_trx_reg12 & ~0xFFFu) | PWR_REGC_0_3(row) | (PWR_REGC_4_7(row) << 4) |
                  (PWR_REGC_8_11(row) << 8);
    trx_reg_write(12, s_trx_reg12);
}

// The 11b LPF corner. reg6 carries a different capacitor calibration for the
// two modulations, and the bank load leaves it on the OFDM one.
// lpf_capcal_q is [9:4] and lpf_capcal_i is [15:10], so both fields at 0x3F is
// the whole of [15:4].
#define LPF_CAPCAL_MASK 0xFFF0u
#define LPF_CAPCAL_11B  0xFFF0u

// rate is the LMAC's own index: 0-3 are the 11b rates, 4-11 the OFDM ones,
// 128-135 the MCS ones. test_mode is the vendor's transmit test hook and no
// caller in this port sets it.
void rwnx_cal_set_txpwr_by_rate(int32_t rate, uint32_t test_mode) {
    (void)test_mode;

    struct phy_channel_info info;
    phy_get_channel(&info, 0);
    uint32_t bandwidth = (info.info1 >> 8) & 0xFF;

    const uint32_t *table = cfg_tab_g;
    unsigned        shift;

    if (bandwidth == PHY_CHNL_BW_20) {
        if (rate <= 3) {
            shift = shift_tab_b[3 - rate];
            table = cfg_tab_b;
        } else if (rate <= 11) {
            shift = shift_tab_g[11 - rate];
        } else if (rate >= 128 && rate <= 135) {
            shift = shift_tab_n20[135 - rate];
        } else {
            return; // a rate no table covers — leave the gain alone
        }
    } else if (rate >= 128 && rate <= 135) {
        shift = shift_tab_n40[135 - rate];
    } else {
        return;
    }

    set_txpwr(table, shift > 31 ? 31 : shift);

    s_trx_reg6 = (s_trx_reg6 & ~LPF_CAPCAL_MASK) | (rate <= 3 ? LPF_CAPCAL_11B : (TRX_REG6_RESET & LPF_CAPCAL_MASK));
    trx_reg_write(6, s_trx_reg6);
}
