#pragma once

#include "platform/soc.h"


#define MDM_CFG_BASE_ADDR (0x01000800)


typedef volatile struct {
    union {
        uint32_t v;
        struct {
            uint32_t rxallowdsss   : 1;  // [0]
            uint32_t rxallowlegacy : 1;  // [1]
            uint32_t rxallowmm     : 1;  // [2]
            uint32_t rxallowgf     : 1;  // [3]
            uint32_t rxallowvht    : 1;  // [4]
            uint32_t reserved_5_7  : 3;  // [7:5]
            uint32_t dupmrcen      : 1;  // [8]
            uint32_t rxstbcen      : 1;  // [9]
            uint32_t reserved_10   : 1;  // [10]
            uint32_t reserved_11   : 1;  // [11]
            uint32_t force20       : 1;  // [12]
            uint32_t force40       : 1;  // [13]
            uint32_t forcenss1     : 1;  // [14]
            uint32_t reserved_15   : 1;  // [15]
            uint32_t psselect20    : 1;  // [16]
            uint32_t psselect40    : 1;  // [17]
            uint32_t reserved_18_31 : 14; // [31:18]
        };
    } rxmodes;       // 0x00

    union {
        uint32_t v;
        struct {
            uint32_t rxon         : 1;   // [0]
            uint32_t agcccaon     : 1;   // [1]
            uint32_t txon         : 1;   // [2]
            uint32_t reserved_3_31 : 29; // [31:3]
        };
    } rxtxpwrctrl;   // 0x04

    union {
        uint32_t v;
        struct {
            uint32_t stotdcompen    : 1;  // [0]
            uint32_t stoslopeesten  : 1;  // [1]
            uint32_t cpefdcompen    : 1;  // [2]
            uint32_t cpeslopeesten  : 1;  // [3]
            uint32_t stofdcompen    : 1;  // [4]
            uint32_t maxptdsynchoff : 6;  // [10:5]
            uint32_t maxntdsynchoff : 6;  // [16:11]
            uint32_t reserved_17_21 : 5;  // [21:17]
            uint32_t stomode        : 1;  // [22]
            uint32_t cpemode        : 1;  // [23]
            uint32_t stoslopelght   : 1;  // [24]
            uint32_t cpeslopelght   : 1;  // [25]
            uint32_t reserved_26_27 : 2;  // [27:26]
            uint32_t cfgcperef      : 1;  // [28]
            uint32_t cfgstoref      : 1;  // [29]
            uint32_t cfgcpewalk     : 1;  // [30]
            uint32_t cfgsto4tdcomp  : 1;  // [31]
        };
    } stocpectrl0;   // 0x08

    union {
        uint32_t v;
        struct {
            uint32_t stoslopeforced : 18; // [17:0]
            uint32_t reserved_18_31 : 14; // [31:18]
        };
    } stocpectrl1;   // 0x0C

    union {
        uint32_t v;
        struct {
            uint32_t cpeslopeforced : 18; // [17:0]
            uint32_t reserved_18_31 : 14; // [31:18]
        };
    } stocpectrl2;   // 0x10

    union {
        uint32_t v;
        struct {
            uint32_t equalcfgsatsb    : 3;  // [2:0]
            uint32_t fddccompen       : 1;  // [3]
            uint32_t fddcsnrthreshold : 8;  // [11:4]
            uint32_t qbpskdetthr      : 4;  // [15:12]
            uint32_t qpbsknsdthr      : 3;  // [18:16]
            uint32_t reserved_19      : 1;  // [19]
            uint32_t fddcweightm2     : 3;  // [22:20]
            uint32_t fddcweightm1     : 3;  // [25:23]
            uint32_t fddcweightp1     : 3;  // [28:26]
            uint32_t fddcweightp2     : 3;  // [31:29]
        };
    } equalctrl;     // 0x14

    union {
        uint32_t v;
        struct {
            uint32_t tdcycrotval20  : 8;  // [7:0]
            uint32_t tdcycrotval40  : 8;  // [15:8]
            uint32_t cfgsmooth      : 2;  // [17:16]
            uint32_t cfgsmoothforce : 1;  // [18]
            uint32_t smoothen       : 1;  // [19]
            uint32_t tdcycrotval80  : 8;  // [27:20]
            uint32_t reserved_28_31 : 4;  // [31:28]
        };
    } smoothctrl;    // 0x18

    union {
        uint32_t v;
        struct {
            uint32_t waithtstf      : 7;  // [6:0]
            uint32_t reserved_7     : 1;  // [7]
            uint32_t delaynormalgi  : 6;  // [13:8]
            uint32_t reserved_14_15 : 2;  // [15:14]
            uint32_t startdc        : 7;  // [22:16]
            uint32_t reserved_23    : 1;  // [23]
            uint32_t starthtdc      : 7;  // [30:24]
            uint32_t dcesten        : 1;  // [31]
        };
    } dcestimctrl;   // 0x1C

    union {
        uint32_t v;
        struct {
            uint32_t tx20diggainlin0 : 7;  // [6:0]
            uint32_t reserved_7      : 1;  // [7]
            uint32_t tx40diggainlin0 : 7;  // [14:8]
            uint32_t reserved_15     : 1;  // [15]
            uint32_t tx80diggainlin0 : 7;  // [22:16]
            uint32_t reserved_23_31  : 9;  // [31:23]
        };
    } fectrl0;       // 0x20

    uint32_t reserved_0x24_0x2c[3]; // 0x24..0x2C

    union {
        uint32_t v;
        struct {
            uint32_t smoothsnrthrmid  : 8;  // [7:0]
            uint32_t smoothsnrthrhigh : 8;  // [15:8]
            uint32_t reserved_16_31   : 16; // [31:16]
        };
    } smoothsnrthr;  // 0x30

    union {
        uint32_t v;
        struct {
            uint32_t propmodecfg    : 1;  // [0]
            uint32_t lsigparitycfg  : 1;  // [1]
            uint32_t rateerrorcfg   : 1;  // [2]
            uint32_t reserved_3     : 1;  // [3]
            uint32_t tdbwforce20    : 1;  // [4]
            uint32_t tdbwforce40    : 1;  // [5]
            uint32_t reserved_6_11  : 6;  // [11:6]
            uint32_t rxiqswapcfg    : 1;  // [12]
            uint32_t rxc2sdisbcfg   : 1;  // [13]
            uint32_t reserved_14_15 : 2;  // [15:14]
            uint32_t tdbwdetweight  : 5;  // [20:16]
            uint32_t reserved_21_31 : 11; // [31:21]
        };
    } rxctrl0;       // 0x34

    union {
        uint32_t v;
        struct {
            uint32_t txstartdelay   : 10; // [9:0]
            uint32_t reserved_10_27 : 18; // [27:10]
            uint32_t txiqswapcfg    : 1;  // [28]
            uint32_t txc2sdisbcfg   : 1;  // [29]
            uint32_t reserved_30_31 : 2;  // [31:30]
        };
    } txctrl0;       // 0x38

    union {
        uint32_t v;
        struct {
            uint32_t rxndbpsmax    : 13; // [12:0]
            uint32_t reserved_13_31 : 19; // [31:13]
        };
    } rxctrl1;       // 0x3C

    union {
        uint32_t v;
        struct {
            uint32_t intlvphyclken  : 1;  // [0]
            uint32_t intlvvtbclken  : 1;  // [1]
            uint32_t intlvmemclken  : 1;  // [2]
            uint32_t qmemclken      : 1;  // [3]
            uint32_t hmemclken      : 1;  // [4]
            uint32_t gmemclken      : 1;  // [5]
            uint32_t chestclken     : 1;  // [6]
            uint32_t fft0clken      : 1;  // [7]
            uint32_t fft0memclken   : 1;  // [8]
            uint32_t fft1clken      : 1;  // [9]
            uint32_t fft1memclken   : 1;  // [10]
            uint32_t fft2clken      : 1;  // [11]
            uint32_t fft2memclken   : 1;  // [12]
            uint32_t fft3clken      : 1;  // [13]
            uint32_t fft3memclken   : 1;  // [14]
            uint32_t vtbclken       : 1;  // [15]
            uint32_t tbeclken       : 1;  // [16]
            uint32_t tdfoestclken   : 1;  // [17]
            uint32_t tdcomp0clken   : 1;  // [18]
            uint32_t tdcomp1clken   : 1;  // [19]
            uint32_t tdcomp2clken   : 1;  // [20]
            uint32_t tdcomp3clken   : 1;  // [21]
            uint32_t equclken       : 1;  // [22]
            uint32_t stocpeclken    : 1;  // [23]
            uint32_t fe0clken       : 1;  // [24]
            uint32_t fe1clken       : 1;  // [25]
            uint32_t fe2clken       : 1;  // [26]
            uint32_t fe3clken       : 1;  // [27]
            uint32_t agcclken       : 1;  // [28]
            uint32_t agcmemclken    : 1;  // [29]
            uint32_t phyrxclken     : 1;  // [30]
            uint32_t phytxclken     : 1;  // [31]
        };
    } clkgatectrl0;  // 0x40

    union {
        uint32_t v;
        struct {
            uint32_t reserved_0_2   : 3;  // [2:0]
            uint32_t mdmbrxclken    : 1;  // [3]
            uint32_t mdmbtxclken    : 1;  // [4]
            uint32_t radartimclken  : 1;  // [5]
            uint32_t reserved_6_31  : 26; // [31:6]
        };
    } clkgatectrl1;  // 0x44

    union {
        uint32_t v;
        struct {
            uint32_t reserved_0_2   : 3;  // [2:0]
            uint32_t agcmemclkctrl  : 1;  // [3]
            uint32_t reserved_4_31  : 28; // [31:4]
        };
    } memclkctrl0;   // 0x48

    union {
        uint32_t v;
        struct {
            uint32_t irqccatimeouten : 1;  // [0]
            uint32_t irqradardeten   : 1;  // [1]
            uint32_t irqphyerroren   : 1;  // [2]
            uint32_t irqcca20sriseen : 1;  // [3]
            uint32_t irqcca20priseen : 1;  // [4]
            uint32_t irqcca20sfallen : 1;  // [5]
            uint32_t irqcca20pfallen : 1;  // [6]
            uint32_t irqrxenden      : 1;  // [7]
            uint32_t irqtxenden      : 1;  // [8]
            uint32_t irqcca40sriseen : 1;  // [9]
            uint32_t irqcca40sfallen : 1;  // [10]
            uint32_t irqtxunderrunen : 1;  // [11]
            uint32_t irqtxoverflowen : 1;  // [12]
            uint32_t irqlsigvaliden  : 1;  // [13]
            uint32_t irqhtsigvaliden : 1;  // [14]
            uint32_t reserved_15_31  : 17; // [31:15]
        };
    } irqctrl;       // 0x4C

    union {
        uint32_t v;
        struct {
            uint32_t irqccatimeoutack : 1;  // [0]
            uint32_t irqradardetack   : 1;  // [1]
            uint32_t irqphyerrorack   : 1;  // [2]
            uint32_t irqcca20sriseack : 1;  // [3]
            uint32_t irqcca20priseack : 1;  // [4]
            uint32_t irqcca20sfallack : 1;  // [5]
            uint32_t irqcca20pfallack : 1;  // [6]
            uint32_t irqrxendack      : 1;  // [7]
            uint32_t irqtxendack      : 1;  // [8]
            uint32_t irqcca40sriseack : 1;  // [9]
            uint32_t irqcca40sfallack : 1;  // [10]
            uint32_t irqtxunderrunack : 1;  // [11]
            uint32_t irqtxoverflowack : 1;  // [12]
            uint32_t irqlsigvalidack  : 1;  // [13]
            uint32_t irqhtsigvalidack : 1;  // [14]
            uint32_t reserved_15_31   : 17; // [31:15]
        };
    } irqack;        // 0x50

    union {
        uint32_t v;
        struct {
            uint32_t ltdcycl2      : 4;  // [3:0]
            uint32_t ltdcycl3      : 4;  // [7:4]
            uint32_t ltdcycl4      : 4;  // [11:8]
            uint32_t reserved_12_31 : 20; // [31:12]
        };
    } tdcyclshtrl0;  // 0x54

    union {
        uint32_t v;
        struct {
            uint32_t httdcycl2     : 4;  // [3:0]
            uint32_t httdcycl3     : 4;  // [7:4]
            uint32_t httdcycl4     : 4;  // [11:8]
            uint32_t reserved_12_31 : 20; // [31:12]
        };
    } tdcyclshtrl1;  // 0x58

    union {
        uint32_t v;
        struct {
            uint32_t scramseed     : 8;  // [7:0]
            uint32_t reserved_8_31 : 24; // [31:8]
        };
    } scramblerctrl; // 0x5C

    union {
        uint32_t v;
        struct {
            uint32_t tbe_count_adjust_20      : 8;  // [7:0]
            uint32_t tbe_count_adjust_40      : 8;  // [15:8]
            uint32_t tbe_count_adjust_80      : 8;  // [23:16]
            uint32_t cross_corr_snr_threshold : 8;  // [31:24]
        };
    } tbectrl0;      // 0x60

    union {
        uint32_t v;
        struct {
            uint32_t peak_search_start_low_snr  : 7;  // [6:0]
            uint32_t reserved_7                 : 1;  // [7]
            uint32_t peak_search_stop_low_snr   : 7;  // [14:8]
            uint32_t reserved_15                : 1;  // [15]
            uint32_t peak_search_start_high_snr : 7;  // [22:16]
            uint32_t reserved_23                : 1;  // [23]
            uint32_t peak_search_stop_high_snr  : 7;  // [30:24]
            uint32_t reserved_31                : 1;  // [31]
        };
    } tbectrl1;      // 0x64

    union {
        uint32_t v;
        struct {
            uint32_t tbe_bias              : 8;  // [7:0]
            uint32_t reserved_8_11         : 4;  // [11:8]
            uint32_t peak_search_delta     : 5;  // [16:12]
            uint32_t reserved_17_19        : 3;  // [19:17]
            uint32_t td_adjust_20_short_gi : 4;  // [23:20]
            uint32_t td_adjust_40_short_gi : 4;  // [27:24]
            uint32_t td_adjust_80_short_gi : 4;  // [31:28]
        };
    } tbectrl2;      // 0x68

    union {
        uint32_t v;
        struct {
            uint32_t auto_corr_compare_ratio_low_snr  : 10; // [9:0]
            uint32_t reserved_10_11                   : 2;  // [11:10]
            uint32_t auto_corr_compare_ratio_high_snr : 10; // [21:12]
            uint32_t reserved_22_31                   : 10; // [31:22]
        };
    } tdfoctrl0;     // 0x6C

    union {
        uint32_t v;
        struct {
            uint32_t auto_corr_plat_sum_start : 9;  // [8:0]
            uint32_t reserved_9_11            : 3;  // [11:9]
            uint32_t auto_corr_plat_sum_end   : 9;  // [20:12]
            uint32_t reserved_21_31           : 11; // [31:21]
        };
    } tdfoctrl1;     // 0x70

    union {
        uint32_t v;
        struct {
            uint32_t auto_corr_plat_fall_search_start : 9;  // [8:0]
            uint32_t reserved_9                       : 1;  // [9]
            uint32_t auto_corr_ph_sum_start           : 9;  // [18:10]
            uint32_t reserved_19                      : 1;  // [19]
            uint32_t auto_corr_ph_sum_end             : 9;  // [28:20]
            uint32_t reserved_29_31                   : 3;  // [31:29]
        };
    } tdfoctrl2;     // 0x74

    union {
        uint32_t v;
        struct {
            uint32_t auto_corr_fall_count    : 4;  // [3:0]
            uint32_t tdfosnr_threshold       : 8;  // [11:4]
            uint32_t tdfo_plat_fall_time_out : 12; // [23:12]
            uint32_t reserved_24_31          : 8;  // [31:24]
        };
    } tdfoctrl3;     // 0x78

    union {
        uint32_t v;
        struct {
            uint32_t intlvphyclkforce  : 1;  // [0]
            uint32_t intlvvtbclkforce  : 1;  // [1]
            uint32_t intlvmemclkforce  : 1;  // [2]
            uint32_t qmemclkforce      : 1;  // [3]
            uint32_t hmemclkforce      : 1;  // [4]
            uint32_t gmemclkforce      : 1;  // [5]
            uint32_t chestclkforce     : 1;  // [6]
            uint32_t fft0clkforce      : 1;  // [7]
            uint32_t fft0memclkforce   : 1;  // [8]
            uint32_t fft1clkforce      : 1;  // [9]
            uint32_t fft1memclkforce   : 1;  // [10]
            uint32_t fft2clkforce      : 1;  // [11]
            uint32_t fft2memclkforce   : 1;  // [12]
            uint32_t fft3clkforce      : 1;  // [13]
            uint32_t fft3memclkforce   : 1;  // [14]
            uint32_t vtbclkforce       : 1;  // [15]
            uint32_t tbeclkforce       : 1;  // [16]
            uint32_t tdfoestclkforce   : 1;  // [17]
            uint32_t tdcomp0clkforce   : 1;  // [18]
            uint32_t tdcomp1clkforce   : 1;  // [19]
            uint32_t tdcomp2clkforce   : 1;  // [20]
            uint32_t tdcomp3clkforce   : 1;  // [21]
            uint32_t equclkforce       : 1;  // [22]
            uint32_t stocpeclkforce    : 1;  // [23]
            uint32_t fe0clkforce       : 1;  // [24]
            uint32_t fe1clkforce       : 1;  // [25]
            uint32_t fe2clkforce       : 1;  // [26]
            uint32_t fe3clkforce       : 1;  // [27]
            uint32_t agcclkforce       : 1;  // [28]
            uint32_t agcmemclkforce    : 1;  // [29]
            uint32_t phyrxclkforce     : 1;  // [30]
            uint32_t phytxclkforce     : 1;  // [31]
        };
    } clkgatefctrl0; // 0x7C

    union {
        uint32_t v;
        struct {
            uint32_t reserved_0_2      : 3;  // [2:0]
            uint32_t mdmbrxclkforce    : 1;  // [3]
            uint32_t mdmbtxclkforce    : 1;  // [4]
            uint32_t radartimclkforce  : 1;  // [5]
            uint32_t reserved_6_31     : 26; // [31:6]
        };
    } clkgatefctrl1; // 0x80

    union {
        uint32_t v;
        struct {
            uint32_t txqdcoffset0  : 8;  // [7:0]
            uint32_t reserved_8_15 : 8;  // [15:8]
            uint32_t txidcoffset0  : 8;  // [23:16]
            uint32_t reserved_24_31 : 8; // [31:24]
        };
    } dcoffset0;     // 0x84

    uint32_t reserved_0x88_0x90[3]; // 0x88–0x890

    union {
        uint32_t v;
        struct {
            uint32_t mdmswreset    : 1;  // [0]
            uint32_t reserved_1_3  : 3;  // [3:1]
            uint32_t fsmswreset    : 1;  // [4]
            uint32_t reserved_5_7  : 3;  // [7:5]
            uint32_t dspswreset    : 1;  // [8]
            uint32_t reserved_9_11 : 3;  // [11:9]
            uint32_t agcswreset    : 1;  // [12]
            uint32_t reserved_13_31 : 19; // [31:13]
        };
    } swreset;       // 0x94
} hw_mdm_cfg_t;

#define hw_mdm_cfg ((volatile hw_mdm_cfg_t *)MDM_CFG_BASE_ADDR)
