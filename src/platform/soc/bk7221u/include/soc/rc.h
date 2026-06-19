#pragma once

#include <stdint.h>
#include <assert.h>
#include "platform/soc.h"


#define REG_RC_BASE                         (0x01050000)


typedef volatile struct {
    // 0x000: CNTL_STAT
    union {
        struct {
            uint32_t CH0_EN: 1;                 // [0]
            uint32_t reserved_1_2: 2;           // [1:2]
            uint32_t RC_EN: 1;                  // [3]
            uint32_t reserved_4_7: 4;           // [4:7]
            uint32_t CH0_LD: 1;                 // [8]
            uint32_t reserved_9_11: 3;          // [9:11]
            uint32_t CH0_SHDN_STAT: 1;          // [12]
            uint32_t reserved_13_15: 3;         // [13:15]
            uint32_t RC_STATE: 3;               // [16:18]
            uint32_t reserved_19_29: 11;        // [19:29]
            uint32_t SPI_RESET: 1;              // [30]
            uint32_t FORCE_ENABLE: 1;           // [31]
        };
        uint32_t v;
    } CNTL_STAT;

    // 0x004: BEKEN_SPI
    union {
        struct {
            uint32_t TRX_REG_STAT: 28;          // [27:0]
            uint32_t PRESCALER: 4;              // [31:28]
        };
        uint32_t v;
    } BEKEN_SPI;

    volatile uint32_t reserved_02_04[3];        // 0x008..0x010

    // 0x014: CH0_OUTPOWER
    union {
        struct {
            uint32_t CH0_OUTPOWER: 8;           // [7:0]
            uint32_t reserved_8_31: 24;         // [8:31]
        };
        uint32_t v;
    } CH0_OUTPOWER;

    volatile uint32_t reserved_06_07[2];        // 0x018..0x01C

    // 0x020: CH0_RX_ONOFF_DELAY
    union {
        struct {
            uint32_t CH0_RX_ON_DELAY: 8;        // [7:0]
            uint32_t reserved_8_15: 8;          // [8:15]
            uint32_t CH0_RX_OFF_DELAY: 8;       // [23:16]
            uint32_t reserved_24_31: 8;         // [24:31]
        };
        uint32_t v;
    } CH0_RX_ONOFF_DELAY;

    volatile uint32_t reserved_09_0a[2];        // 0x024..0x028

    // 0x02C: CH0_TX_ONOFF_DELAY
    union {
        struct {
            uint32_t CH0_TX_ON_DELAY: 8;        // [7:0]
            uint32_t reserved_8_15: 8;          // [8:15]
            uint32_t CH0_TX_OFF_DELAY: 8;       // [23:16]
            uint32_t reserved_24_31: 8;         // [24:31]
        };
        uint32_t v;
    } CH0_TX_ONOFF_DELAY;

    volatile uint32_t reserved_0c_0d[2];        // 0x030..0x034

    // 0x038: CH0_PA_ONOFF_DELAY
    union {
        struct {
            uint32_t CH0_PA_ON_DELAY: 8;        // [7:0]
            uint32_t reserved_8_15: 8;          // [8:15]
            uint32_t CH0_PA_OFF_DELAY: 8;       // [23:16]
            uint32_t reserved_24_31: 8;         // [24:31]
        };
        uint32_t v;
    } CH0_PA_ONOFF_DELAY;

    volatile uint32_t reserved_0f_10[2];        // 0x03C..0x040

    // 0x044: CH0_SHDN_ONOFF_DELAY
    union {
        struct {
            uint32_t CH0_SHDN_ON_DELAY: 8;      // [7:0]
            uint32_t reserved_8_15: 8;          // [8:15]
            uint32_t CH0_SHDN_OFF_DELAY: 8;     // [23:16]
            uint32_t reserved_24_31: 8;         // [24:31]
        };
        uint32_t v;
    } CH0_SHDN_ONOFF_DELAY;

    volatile uint32_t reserved_12_18[7];        // 0x048..0x060

    // 0x064: CH0_FORCE
    union {
        struct {
            uint32_t F_CH0_SHDN: 1;             // [0]
            uint32_t F_CH0_RXEN: 1;             // [1]
            uint32_t F_CH0_TXEN: 1;             // [2]
            uint32_t F_CH0_RXHP: 1;             // [3]
            uint32_t reserved_4_7: 4;           // [4:7]
            uint32_t F_CH0_B: 8;                // [15:8]
            uint32_t F_CH0_EN: 1;               // [16]
            uint32_t reserved_17_31: 15;        // [17:31]
        };
        uint32_t v;
    } CH0_FORCE;

    volatile uint32_t reserved_1a_1b[2];        // 0x068..0x06C

    // 0x070: MISC_FORCE
    union {
        struct {
            uint32_t F_RX_ON: 1;                // [0]
            uint32_t F_TX_ON: 1;                // [1]
            uint32_t reserved_2_31: 30;         // [2:31]
        };
        uint32_t v;
    } MISC_FORCE;

    volatile uint32_t reserved_1d;              // 0x074

    // 0x078: FE_RX_DEL
    union {
        struct {
            uint32_t FE_RX_ON_DEL: 12;          // [11:0]
            uint32_t reserved_12_31: 20;        // [12:31]
        };
        uint32_t v;
    } FE_RX_DEL;

    volatile uint32_t reserved_1f;              // 0x07C

    // 0x080..0x0EC: TRX_REG0..TRX_REG27
    volatile uint32_t TRX_REG0;                 // 0x080
    volatile uint32_t TRX_REG1;                 // 0x084
    volatile uint32_t TRX_REG2;                 // 0x088
    volatile uint32_t TRX_REG3;                 // 0x08C
    volatile uint32_t TRX_REG4;                 // 0x090
    volatile uint32_t TRX_REG5;                 // 0x094
    volatile uint32_t TRX_REG6;                 // 0x098
    volatile uint32_t TRX_REG7;                 // 0x09C
    volatile uint32_t TRX_REG8;                 // 0x0A0
    volatile uint32_t TRX_REG9;                 // 0x0A4
    volatile uint32_t TRX_REG10;                // 0x0A8
    volatile uint32_t TRX_REG11;                // 0x0AC
    volatile uint32_t TRX_REG12;                // 0x0B0
    volatile uint32_t TRX_REG13;                // 0x0B4
    volatile uint32_t TRX_REG14;                // 0x0B8
    volatile uint32_t TRX_REG15;                // 0x0BC
    volatile uint32_t TRX_REG16;                // 0x0C0
    volatile uint32_t TRX_REG17;                // 0x0C4
    volatile uint32_t TRX_REG18;                // 0x0C8
    volatile uint32_t TRX_REG19;                // 0x0CC
    volatile uint32_t TRX_REG20;                // 0x0D0
    volatile uint32_t TRX_REG21;                // 0x0D4
    volatile uint32_t TRX_REG22;                // 0x0D8
    volatile uint32_t TRX_REG23;                // 0x0DC
    volatile uint32_t TRX_REG24;                // 0x0E0
    volatile uint32_t TRX_REG25;                // 0x0E4
    volatile uint32_t TRX_REG26;                // 0x0E8
    volatile uint32_t TRX_REG27;                // 0x0EC

    // 0x0F0: RX_AVG_CALC
    union {
        struct {
            uint32_t RX_AVG_Q_RD: 12;           // [11:0]
            uint32_t RX_AVG_I_RD: 12;           // [23:12]
            uint32_t reserved_24_29: 6;         // [24:29]
            uint32_t RX_AVG_MODE: 1;            // [30]
            uint32_t RX_DC_CAL_EN: 1;           // [31]
        };
        uint32_t v;
    } RX_AVG_CALC;

    volatile uint32_t reserved_3d;              // 0x0F4

    // 0x0F8: RX_CALIB_EN
    union {
        struct {
            uint32_t RX_DC_Q_RD: 12;            // [11:0]
            uint32_t RX_DC_I_RD: 12;            // [23:12]
            uint32_t reserved_24_29: 6;         // [24:29]
            uint32_t RX_COMP_EN: 1;             // [30]
            uint32_t RX_CAL_EN: 1;              // [31]
        };
        uint32_t v;
    } RX_CALIB_EN;

    // 0x0FC: RX_ERROR_RD
    union {
        struct {
            uint32_t RX_PHASE_ERR_RD: 10;       // [9:0]
            uint32_t reserved_10_15: 6;         // [10:15]
            uint32_t RX_AMP_ERR_RD: 10;         // [25:16]
            uint32_t reserved_26_31: 6;         // [26:31]
        };
        uint32_t v;
    } RX_ERROR_RD;

    // 0x100: RX_TY2_RD
    union {
        struct {
            uint32_t RX_TY2_RD: 10;             // [9:0]
            uint32_t reserved_10_31: 22;        // [10:31]
        };
        uint32_t v;
    } RX_TY2_RD;

    // 0x104: RX_DC_WR
    union {
        struct {
            uint32_t RX_DC_Q_WR: 12;            // [11:0]
            uint32_t RX_DC_I_WR: 12;            // [23:12]
            uint32_t reserved_24_31: 8;         // [24:31]
        };
        uint32_t v;
    } RX_DC_WR;

    // 0x108: RX_ERROR_WR
    union {
        struct {
            uint32_t RX_PHASE_ERR_WR: 10;       // [9:0]
            uint32_t reserved_10_15: 6;         // [10:15]
            uint32_t RX_AMP_ERR_WR: 10;         // [25:16]
            uint32_t reserved_26_31: 6;         // [26:31]
        };
        uint32_t v;
    } RX_ERROR_WR;

    volatile uint32_t reserved_43_4b[9];        // 0x10C..0x12C

    // 0x130: TX_MODE_CFG
    union {
        struct {
            uint32_t Q_CONST: 10;               // [9:0]
            uint32_t reserved_10_15: 6;         // [10:15]
            uint32_t I_CONST: 10;               // [25:16]
            uint32_t reserved_26_29: 4;         // [26:29]
            uint32_t TEST_PATTERN: 2;           // [31:30]
        };
        uint32_t v;
    } TX_MODE_CFG;

    // 0x134: TX_SIN_CFG
    union {
        struct {
            uint32_t reserved_0_11: 12;         // [11:0]
            uint32_t TX_SIN_AMP: 4;             // [15:12]
            uint32_t TX_SIN_MODE: 2;            // [17:16]
            uint32_t reserved_18_21: 4;         // [18:21]
            uint32_t TX_SIN_F: 10;              // [31:22]
        };
        uint32_t v;
    } TX_SIN_CFG;

    volatile uint32_t reserved_4e;              // 0x138

    // 0x13C: TX_DC_COMP
    union {
        struct {
            uint32_t TX_Q_DC_COMP: 10;          // [9:0]
            uint32_t reserved_10_15: 6;         // [10:15]
            uint32_t TX_I_DC_COMP: 10;          // [25:16]
            uint32_t reserved_26_31: 6;         // [26:31]
        };
        uint32_t v;
    } TX_DC_COMP;

    // 0x140: TX_GAIN_COMP
    union {
        struct {
            uint32_t TX_Q_GAIN_COMP: 10;        // [9:0]
            uint32_t reserved_10_15: 6;         // [10:15]
            uint32_t TX_I_GAIN_COMP: 10;        // [25:16]
            uint32_t reserved_26_31: 6;         // [26:31]
        };
        uint32_t v;
    } TX_GAIN_COMP;

    // 0x144: TX_PHASE_TY2_COMP
    union {
        struct {
            uint32_t TX_TY2_COMP: 10;           // [9:0]
            uint32_t reserved_10_15: 6;         // [10:15]
            uint32_t TX_PHASE_COMP: 10;         // [25:16]
            uint32_t reserved_26_31: 6;         // [26:31]
        };
        uint32_t v;
    } TX_PHASE_TY2_COMP;

    // 0x148: TX_OTHER_CFG
    union {
        struct {
            uint32_t reserved_0_5: 6;           // [5:0]
            uint32_t IQ_CONSTANT_IQCAL_P: 10;   // [15:6]
            uint32_t TX_PRE_GAIN: 4;            // [19:16]
            uint32_t reserved_20: 1;            // [20]
            uint32_t IQ_CONSTANT_POUT: 10;      // [30:21]
            uint32_t TX_IQ_SWAP: 1;             // [31]
        };
        uint32_t v;
    } TX_OTHER_CFG;

    volatile uint32_t reserved_53;              // 0x14C

    // 0x150: AGC_CFG
    union {
        struct {
            uint32_t ST_RX_ADC_IQ: 1;           // [0]
            uint32_t TSSI_POUT_TH: 8;           // [8:1]
            uint32_t DSEL_VA: 1;                // [9]
            uint32_t TX_DC_N: 2;                // [11:10]
            uint32_t ST_RX_ADC: 2;              // [13:12]
            uint32_t ST_SAR_ADC: 2;             // [15:14]
            uint32_t TSSI_RD: 8;                // [23:16]
            uint32_t AGC_PGA_RD: 4;             // [27:24]
            uint32_t AGC_BUF_RD: 1;             // [28]
            uint32_t AGC_LNA_RD: 2;             // [30:29]
            uint32_t AGC_MAP_MODE: 1;           // [31]
        };
        uint32_t v;
    } AGC_CFG;

    volatile uint32_t reserved_55_5b[7];        // 0x154..0x16C

    // 0x170: TRX_SPI_INTLV
    union {
        struct {
            uint32_t TRX_SPI_INTLV: 10;         // [9:0]
            uint32_t reserved_10_30: 21;        // [10:30]
            uint32_t TRX_BANK_RPT: 1;           // [31]
        };
        uint32_t v;
    } TRX_SPI_INTLV;

    volatile uint32_t reserved_5d_63[7];        // 0x174..0x18C

    // 0x190..0x1A4: ADDA_REG0..ADDA_REG5 (opaque 32-bit values, no documented internal structure)
    volatile uint32_t ADDA_REG0;                // 0x190
    volatile uint32_t ADDA_REG1;                // 0x194
    volatile uint32_t ADDA_REG2;                // 0x198
    volatile uint32_t ADDA_REG3;                // 0x19C
    volatile uint32_t ADDA_REG4;                // 0x1A0
    volatile uint32_t ADDA_REG5;                // 0x1A4

    // 0x1A8: ADDA_REG_STAT
    union {
        struct {
            uint32_t ADDA_REG_STAT: 6;          // [5:0]
            uint32_t reserved_6_31: 26;         // [6:31]
        };
        uint32_t v;
    } ADDA_REG_STAT;
} hw_rc_reg_t;

static_assert(sizeof (hw_rc_reg_t) == 428, "");


#define hw_rc               ((volatile hw_rc_reg_t *) REG_RC_BASE)


#define RC_TRX_REG28_ADDR          0x08628078

inline void rc_trx_reg28_set(uint32_t value) {
    *((volatile uint32_t *) RC_TRX_REG28_ADDR) = value;
}
