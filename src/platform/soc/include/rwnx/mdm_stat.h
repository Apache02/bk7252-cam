#pragma once

#include "platform/soc.h"


#define MDM_STAT_BASE_ADDR (0x01000000)


typedef volatile struct {
    union {
        uint32_t v;
        struct {
            uint32_t phy_config : 32;        // [31:0]
        };
    } nxversion;                             // 0x00

    union {
        uint32_t v;
        struct {
            uint32_t coarsefoangle : 21;     // [20:0]
            uint32_t reserved_21_31 : 11;    // [31:21]
        };
    } tdoffsetstat0;                         // 0x04

    union {
        uint32_t v;
        struct {
            uint32_t finefoangle : 21;       // [20:0]
            uint32_t reserved_21_31 : 11;    // [31:21]
        };
    } tdoffsetstat1;                         // 0x08

    union {
        uint32_t v;
        struct {
            uint32_t tdsynchoffstat : 6;     // [5:0]
            uint32_t reserved_6_11 : 6;      // [11:6]
            uint32_t stoslopestat : 18;      // [29:12]
            uint32_t reserved_30_31 : 2;     // [31:30]
        };
    } fdtoffsetstat0;                        // 0x0C

    union {
        uint32_t v;
        struct {
            uint32_t cpeslopestat : 18;      // [17:0]
            uint32_t reserved_18_31 : 14;    // [31:18]
        };
    } fdtoffsetstat1;                        // 0x10

    union {
        uint32_t v;
        struct {
            uint32_t tdsymbolnr : 16;        // [15:0]
            uint32_t fdsymbolnr : 16;        // [31:16]
        };
    } rxfsmstat0;                            // 0x14

    union {
        uint32_t v;
        struct {
            uint32_t fdsymboltype : 3;       // [2:0]
            uint32_t reserved_3 : 1;         // [3]
            uint32_t rxmodestat : 3;         // [6:4]
            uint32_t reserved_7 : 1;         // [7]
            uint32_t fdbwreg : 2;            // [9:8]
            uint32_t tdbwreg : 2;            // [11:10]
            uint32_t reserved_12 : 1;        // [12]
            uint32_t stbcen : 1;             // [13]
            uint32_t reserved_14_15 : 2;     // [15:14]
            uint32_t gilength : 6;           // [21:16]
            uint32_t reserved_22_23 : 2;     // [23:22]
            uint32_t fdstate : 8;            // [31:24]
        };
    } rxfsmstat1;                            // 0x18

    union {
        uint32_t v;
        struct {
            uint32_t phy_tx_abort_stat : 1;  // [0]
            uint32_t reserved_1_31 : 31;     // [31:1]
        };
    } txfsmstat1;                            // 0x1C

    union {
        uint32_t v;
        struct {
            uint32_t lsigparity : 1;         // [0]
            uint32_t htsigcrc : 1;           // [1]
            uint32_t rxformaterr : 1;         // [2]
            uint32_t agcunlock : 1;          // [3]
            uint32_t txunderrun : 1;         // [4]
            uint32_t txformaterr : 1;        // [5]
            uint32_t txoverflow : 1;         // [6]
            uint32_t reserved_7_31 : 25;     // [31:7]
        };
    } errstat;                               // 0x20

    union {
        uint32_t v;
        struct {
            uint32_t irqccatimeout : 1;      // [0]
            uint32_t irqradardet : 1;        // [1]
            uint32_t irqphyerror : 1;        // [2]
            uint32_t irqcca20srise : 1;      // [3]
            uint32_t irqcca20prise : 1;      // [4]
            uint32_t irqcca20sfall : 1;      // [5]
            uint32_t irqcca20pfall : 1;      // [6]
            uint32_t irqrxend : 1;           // [7]
            uint32_t irqtxend : 1;           // [8]
            uint32_t irqcca40srise : 1;      // [9]
            uint32_t irqcca40sfall : 1;      // [10]
            uint32_t irqtxunderrun : 1;      // [11]
            uint32_t irqtxoverflow : 1;      // [12]
            uint32_t irqlsigvalid : 1;       // [13]
            uint32_t irqhtsigvalid : 1;      // [14]
            uint32_t reserved_15_31 : 17;    // [31:15]
        };
    } irqstat;                               // 0x24

    union {
        uint32_t v;
        struct {
            uint32_t rxleglength : 12;       // [11:0]
            uint32_t rxlegrate : 4;          // [15:12]
            uint32_t reserved_16_31 : 16;    // [31:16]
        };
    } rxvector0;                             // 0x28

    union {
        uint32_t v;
        struct {
            uint32_t rxhtlength : 20;        // [19:0]
            uint32_t rxshortgi : 1;          // [20]
            uint32_t rxstbc : 2;             // [22:21]
            uint32_t rxsmoothing : 1;        // [23]
            uint32_t rxmcs : 7;              // [30:24]
            uint32_t rxpretype : 1;          // [31]
        };
    } rxvector1;                             // 0x2C

    union {
        uint32_t v;
        struct {
            uint32_t rxformat : 3;           // [2:0]
            uint32_t reserved_3 : 1;         // [3]
            uint32_t rxchbw : 2;             // [5:4]
            uint32_t reserved_6_7 : 2;       // [7:6]
            uint32_t rxnumsts : 3;           // [10:8]
            uint32_t reserved_11 : 1;        // [11]
            uint32_t rxsounding : 1;         // [12]
            uint32_t reserved_13_15 : 3;     // [15:13]
            uint32_t rxnumextss : 2;         // [17:16]
            uint32_t reserved_18_19 : 2;     // [19:18]
            uint32_t rxaggreg : 1;           // [20]
            uint32_t rxfeccoding : 1;        // [21]
            uint32_t rxdynbw : 1;            // [22]
            uint32_t rxdozenotallowed : 1;   // [23]
            uint32_t rxantennaset : 8;       // [31:24]
        };
    } rxvector2;                             // 0x30

    union {
        uint32_t v;
        struct {
            uint32_t rssi1 : 8;              // [7:0]
            uint32_t rssi2 : 8;              // [15:8]
            uint32_t rssi3 : 8;              // [23:16]
            uint32_t rssi4 : 8;              // [31:24]
        };
    } rxvector3;                             // 0x34

    union {
        uint32_t v;
        struct {
            uint32_t rcpi : 8;               // [7:0]
            uint32_t reserved_8_11 : 4;      // [11:8]
            uint32_t rxpartialaid : 9;       // [20:12]
            uint32_t reserved_21_23 : 3;     // [23:21]
            uint32_t rxgroupid : 6;          // [29:24]
            uint32_t reserved_30_31 : 2;     // [31:30]
        };
    } rxvector4;                             // 0x38

    union {
        uint32_t v;
        struct {
            uint32_t evm1 : 8;               // [7:0]
            uint32_t evm2 : 8;               // [15:8]
            uint32_t evm3 : 8;               // [23:16]
            uint32_t evm4 : 8;               // [31:24]
        };
    } rxvector5;                             // 0x3C

    union {
        uint32_t v;
        struct {
            uint32_t txpwlevel : 8;          // [7:0]
            uint32_t txsounding : 1;         // [8]
            uint32_t txfeccoding : 1;        // [9]
            uint32_t txdynbw : 1;            // [10]
            uint32_t txsmoothing : 1;        // [11]
            uint32_t txchbw : 2;             // [13:12]
            uint32_t reserved_14_15 : 2;     // [15:14]
            uint32_t txantennaset : 8;       // [23:16]
            uint32_t smmindex : 8;           // [31:24]
        };
    } txvector0;                             // 0x40

    union {
        uint32_t v;
        struct {
            uint32_t txmcs : 7;              // [6:0]
            uint32_t reserved_7 : 1;         // [7]
            uint32_t pretype : 1;            // [8]
            uint32_t reserved_9_11 : 3;      // [11:9]
            uint32_t txformat : 3;           // [14:12]
            uint32_t reserved_15 : 1;        // [15]
            uint32_t txnumextss : 2;         // [17:16]
            uint32_t reserved_18_19 : 2;     // [19:18]
            uint32_t txnumsts : 3;           // [22:20]
            uint32_t reserved_23_31 : 9;     // [31:23]
        };
    } txvector1;                             // 0x44

    union {
        uint32_t v;
        struct {
            uint32_t txleglentgh : 12;       // [11:0]  SDK spells it "lentgh"
            uint32_t txlegrate : 4;          // [15:12]
            uint32_t service : 16;           // [31:16]
        };
    } txvector2;                             // 0x48

    union {
        uint32_t v;
        struct {
            uint32_t txhtlength : 20;        // [19:0]
            uint32_t txaggreg : 1;           // [20]
            uint32_t txdozenotallowed : 1;   // [21]
            uint32_t txshortgi : 1;          // [22]
            uint32_t reserved_23 : 1;        // [23]
            uint32_t ntx : 3;                // [26:24]
            uint32_t reserved_27 : 1;        // [27]
            uint32_t txstbc : 2;             // [29:28]
            uint32_t reserved_30_31 : 2;     // [31:30]
        };
    } txvector3;                             // 0x4C

    union {
        uint32_t v;
        struct {
            uint32_t txpartialaid : 9;       // [8:0]
            uint32_t reserved_9_11 : 3;      // [11:9]
            uint32_t txgroupid : 6;          // [17:12]
            uint32_t reserved_18_31 : 14;    // [31:18]
        };
    } txvector4;                             // 0x50

    union {
        uint32_t v;
        struct {
            uint32_t txvecsounderr : 1;      // [0]
            uint32_t txleclenerr : 1;        // [1]
            uint32_t txvecnullerr : 1;       // [2]
            uint32_t txveclegerr : 1;        // [3]
            uint32_t txvecmcserr : 1;        // [4]
            uint32_t mtxvecgi1err : 1;       // [5]
            uint32_t txvecbwerr : 1;         // [6]
            uint32_t txvecgi2err : 1;        // [7]
            uint32_t reserved_8_31 : 24;     // [31:8]
        };
    } txvecdecstat;                          // 0x54

    union {
        uint32_t v;
        struct {
            uint32_t mimocmd : 8;            // [7:0]
            uint32_t mimocmdp1 : 8;          // [15:8]
            uint32_t mimocmdp2 : 8;          // [23:16]
            uint32_t mimocmdp3 : 8;          // [31:24]
        };
    } mimocmdstat;                           // 0x58

    union {
        uint32_t v;
        struct {
            uint32_t tbecount : 8;           // [7:0]
            uint32_t tbe_max_corr_val_sum : 16; // [23:8]
            uint32_t reserved_24_31 : 8;     // [31:24]
        };
    } tbestat0;                              // 0x5C

    union {
        uint32_t v;
        struct {
            uint32_t agcgain0 : 7;           // [6:0]
            uint32_t reserved_7 : 1;         // [7]
            uint32_t agcgain1 : 7;           // [14:8]
            uint32_t reserved_15 : 1;        // [15]
            uint32_t agcgain2 : 7;           // [22:16]
            uint32_t reserved_23 : 1;        // [23]
            uint32_t agcgain3 : 7;           // [30:24]
            uint32_t reserved_31 : 1;        // [31]
        };
    } agcgainstat;                           // 0x60

    union {
        uint32_t v;
        struct {
            uint32_t adcpowdbv0 : 7;         // [6:0]
            uint32_t reserved_7 : 1;         // [7]
            uint32_t adcpowdbv1 : 7;         // [14:8]
            uint32_t reserved_15 : 1;        // [15]
            uint32_t adcpowdbv2 : 7;         // [22:16]
            uint32_t reserved_23 : 1;        // [23]
            uint32_t adcpowdbv3 : 7;         // [30:24]
            uint32_t reserved_31 : 1;        // [31]
        };
    } agcadcpowstat;                         // 0x64

    union {
        uint32_t v;
        struct {
            uint32_t radfifoempty : 1;       // [0]
            uint32_t reserved_1_31 : 31;     // [31:1]
        };
    } radarfifostat;                         // 0x68
} hw_mdm_stat_t;

#define hw_mdm_stat ((volatile hw_mdm_stat_t *)MDM_STAT_BASE_ADDR)
