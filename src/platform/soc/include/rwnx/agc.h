#pragma once

#include "platform/soc.h"


#define AGC_BASE_ADDR (0x01002000)


typedef volatile struct {

    union {
        uint32_t v;
        struct {
            uint32_t rfgainmindb: 7;        // [6:0]
            uint32_t reserved_7: 1;         // [7]
            uint32_t rfgainmaxdb: 7;        // [14:8]
            uint32_t reserved_15: 1;        // [15]
            uint32_t rflossant0db: 4;       // [19:16]
            uint32_t rflossant1db: 4;       // [23:20]
            uint32_t rflossant2db: 4;       // [27:24]
            uint32_t rflossant3db: 4;       // [31:28]
        };
    } gainrg;      // 0x00

    union {
        uint32_t v;
        struct {
            uint32_t lnaldb: 7;             // [6:0]
            uint32_t reserved_7: 1;         // [7]
            uint32_t lmamdb: 6;             // [13:8]
            uint32_t reserved_14_15: 2;     // [15:14]
            uint32_t lnahdb: 6;             // [21:16]
            uint32_t reserved_22_23: 2;     // [23:22]
            uint32_t vgamindb: 4;           // [27:24]
            uint32_t vgastepdb: 3;          // [30:28]
            uint32_t reserved_31: 1;        // [31]
        };
    } lnavgaref0;  // 0x04

    union {
        uint32_t v;
        struct {
            uint32_t lnamlthrdb: 6;         // [5:0]
            uint32_t reserved_6_7: 2;       // [7:6]
            uint32_t lnahmthrdb: 6;         // [13:8]
            uint32_t reserved_14_15: 2;     // [15:14]
            uint32_t vgaindmax: 5;          // [20:16]
            uint32_t reserved_21_23: 3;     // [23:21]
            uint32_t diggain40vs20: 4;      // [27:24]
            uint32_t diggain80vs20: 4;      // [31:28]
        };
    } lnavgaref1;  // 0x08

    union {
        uint32_t v;
        struct {
            uint32_t aci20margmcs0: 6;      // [5:0]
            uint32_t reserved_6_7: 2;       // [7:6]
            uint32_t aci20margmcs1: 6;      // [13:8]
            uint32_t reserved_14_15: 2;     // [15:14]
            uint32_t aci20margmcs2: 6;      // [21:16]
            uint32_t reserved_22_23: 2;     // [23:22]
            uint32_t aci20margmcs3: 6;      // [29:24]
            uint32_t reserved_30_31: 2;     // [31:30]
        };
    } aci20marg0;  // 0x0C

    union {
        uint32_t v;
        struct {
            uint32_t aci20margmcs4: 6;      // [5:0]
            uint32_t reserved_6_7: 2;       // [7:6]
            uint32_t aci20margmcs5: 6;      // [13:8]
            uint32_t reserved_14_15: 2;     // [15:14]
            uint32_t aci20margmcs6: 6;      // [21:16]
            uint32_t reserved_22_23: 2;     // [23:22]
            uint32_t aci20margmcs7: 6;      // [29:24]
            uint32_t reserved_30_31: 2;     // [31:30]
        };
    } aci20marg1;  // 0x10

    union {
        uint32_t v;
        struct {
            uint32_t aci20margmcs8: 6;      // [5:0]
            uint32_t reserved_6_7: 2;       // [7:6]
            uint32_t aci20margmcs9: 6;      // [13:8]
            uint32_t reserved_14_31: 18;    // [31:14]
        };
    } aci20marg2;  // 0x14

    union {
        uint32_t v;
        struct {
            uint32_t aci40margmcs0: 6;      // [5:0]
            uint32_t reserved_6_7: 2;       // [7:6]
            uint32_t aci40margmcs1: 6;      // [13:8]
            uint32_t reserved_14_15: 2;     // [15:14]
            uint32_t aci40margmcs2: 6;      // [21:16]
            uint32_t reserved_22_23: 2;     // [23:22]
            uint32_t aci40margmcs3: 6;      // [29:24]
            uint32_t reserved_30_31: 2;     // [31:30]
        };
    } aci40marg0;  // 0x18

    union {
        uint32_t v;
        struct {
            uint32_t aci40margmcs4: 6;      // [5:0]
            uint32_t reserved_6_7: 2;       // [7:6]
            uint32_t aci40margmcs5: 6;      // [13:8]
            uint32_t reserved_14_15: 2;     // [15:14]
            uint32_t aci40margmcs6: 6;      // [21:16]
            uint32_t reserved_22_23: 2;     // [23:22]
            uint32_t aci40margmcs7: 6;      // [29:24]
            uint32_t reserved_30_31: 2;     // [31:30]
        };
    } aci40marg1;  // 0x1C

    union {
        uint32_t v;
        struct {
            uint32_t aci40margmcs8: 6;      // [5:0]
            uint32_t reserved_6_7: 2;       // [7:6]
            uint32_t aci40margmcs9: 6;      // [13:8]
            uint32_t reserved_14_31: 18;    // [31:14]
        };
    } aci40marg2;  // 0x20

    union {
        uint32_t v;
        struct {
            uint32_t aci80margmcs0: 6;      // [5:0]
            uint32_t reserved_6_7: 2;       // [7:6]
            uint32_t aci80margmcs1: 6;      // [13:8]
            uint32_t reserved_14_15: 2;     // [15:14]
            uint32_t aci80margmcs2: 6;      // [21:16]
            uint32_t reserved_22_23: 2;     // [23:22]
            uint32_t aci80margmcs3: 6;      // [29:24]
            uint32_t reserved_30_31: 2;     // [31:30]
        };
    } aci80marg0;  // 0x24

    union {
        uint32_t v;
        struct {
            uint32_t aci80margmcs4: 6;      // [5:0]
            uint32_t reserved_6_7: 2;       // [7:6]
            uint32_t aci80margmcs5: 6;      // [13:8]
            uint32_t reserved_14_15: 2;     // [15:14]
            uint32_t aci80margmcs6: 6;      // [21:16]
            uint32_t reserved_22_23: 2;     // [23:22]
            uint32_t aci80margmcs7: 6;      // [29:24]
            uint32_t reserved_30_31: 2;     // [31:30]
        };
    } aci80marg1;  // 0x28

    union {
        uint32_t v;
        struct {
            uint32_t aci80margmcs8: 6;      // [5:0]
            uint32_t reserved_6_7: 2;       // [7:6]
            uint32_t aci80margmcs9: 6;      // [13:8]
            uint32_t reserved_14_31: 18;    // [31:14]
        };
    } aci80marg2;  // 0x2C

    union {
        uint32_t v;
        struct {
            uint32_t chgvganlthr: 5;        // [4:0]
            uint32_t reserved_5_7: 3;       // [7:5]
            uint32_t chgvgansthr: 5;        // [12:8]
            uint32_t reserved_13_15: 3;     // [15:13]
            uint32_t chgvgaplthr: 5;        // [20:16]
            uint32_t reserved_21_23: 3;     // [23:21]
            uint32_t chgvgapsthr: 5;        // [28:24]
            uint32_t reserved_29_31: 3;     // [31:29]
        };
    } cgh;         // 0x30

    union {
        uint32_t v;
        struct {
            uint32_t satthrdbv: 6;          // [5:0]
            uint32_t reserved_6_7: 2;       // [7:6]
            uint32_t satlowthrdbv: 6;       // [13:8]
            uint32_t reserved_14_15: 2;     // [15:14]
            uint32_t sathighthrdbv: 6;      // [21:16]
            uint32_t reserved_22_23: 2;     // [23:22]
            uint32_t satdelay50ns: 5;       // [28:24]
            uint32_t reserved_29_31: 3;     // [31:29]
        };
    } sat;         // 0x34

    union {
        uint32_t v;
        struct {
            uint32_t crossupthrqdbm: 10;    // [9:0]
            uint32_t reserved_10_11: 2;     // [11:10]
            uint32_t crossdnthrqdbm: 10;    // [21:12]
            uint32_t reserved_22_31: 10;    // [31:22]
        };
    } cross;       // 0x38

    union {
        uint32_t v;
        struct {
            uint32_t rampupgapqdb: 8;       // [7:0]
            uint32_t rampupndlindex: 3;     // [10:8]
            uint32_t reserved_11_15: 5;     // [15:11]
            uint32_t rampdngapqdb: 8;       // [23:16]
            uint32_t rampdnndlindex: 3;     // [26:24]
            uint32_t reserved_27_31: 5;     // [31:27]
        };
    } ramp;        // 0x3C

    union {
        uint32_t v;
        struct {
            uint32_t platdpqdb: 5;          // [4:0]
            uint32_t reserved_5_7: 3;       // [7:5]
            uint32_t platndlindex: 3;       // [10:8]
            uint32_t reserved_11_15: 5;     // [15:11]
            uint32_t adcpowdisthrdbv: 7;    // [22:16]
            uint32_t reserved_23: 1;        // [23]
            uint32_t adcpowdisndl: 4;       // [27:24]
            uint32_t reserved_28_31: 4;     // [31:28]
        };
    } stabdis;     // 0x40

    union {
        uint32_t v;
        struct {
            uint32_t dcadczerotime50ns: 7;  // [6:0]
            uint32_t reserved_7: 1;         // [7]
            uint32_t dcadcholdtime50ns: 7;  // [14:8]
            uint32_t reserved_15: 1;        // [15]
            uint32_t dcadctype: 2;          // [17:16]
            uint32_t reserved_18_19: 2;     // [19:18]
            uint32_t dcagc20type: 2;        // [21:20]
            uint32_t reserved_22_23: 2;     // [23:22]
            uint32_t dcmdm20type: 2;        // [25:24]
            uint32_t reserved_26_27: 2;     // [27:26]
            uint32_t dcradartype: 2;        // [29:28]
            uint32_t reserved_30_31: 2;     // [31:30]
        };
    } dccomp0;     // 0x44

    union {
        uint32_t v;
        struct {
            uint32_t dcagc20zerotime50ns: 7;  // [6:0]
            uint32_t reserved_7: 1;           // [7]
            uint32_t dcagc20holdtime50ns: 7;  // [14:8]
            uint32_t reserved_15: 1;          // [15]
            uint32_t dcmdm20zerotime50ns: 7;  // [22:16]
            uint32_t reserved_23: 1;          // [23]
            uint32_t dcmdm20holdtime50ns: 7;  // [30:24]
            uint32_t reserved_31: 1;          // [31]
        };
    } dccomp1;     // 0x48

    union {
        uint32_t v;
        struct {
            uint32_t deldcstablecc: 7;        // [6:0]
            uint32_t reserved_7: 1;           // [7]
            uint32_t dcradarzerotime50ns: 7;  // [14:8]
            uint32_t reserved_15: 1;          // [15]
            uint32_t dcradarholdtime50ns: 7;  // [22:16]
            uint32_t reserved_23_31: 9;       // [31:23]
        };
    } dccomp2;     // 0x4C

    union {
        uint32_t v;
        struct {
            uint32_t evt0tgtadd: 11;        // [10:0]
            uint32_t evt0opcomb: 2;         // [12:11]
            uint32_t reserved_13: 1;        // [13]
            uint32_t evt0op1: 6;            // [19:14]
            uint32_t evt0op2: 6;            // [25:20]
            uint32_t evt0op3: 6;            // [31:26]
        };
    } evtsat;      // 0x50

    union {
        uint32_t v;
        struct {
            uint32_t evt1tgtadd: 11;        // [10:0]
            uint32_t evt1opcomb: 2;         // [12:11]
            uint32_t reserved_13: 1;        // [13]
            uint32_t evt1op1: 6;            // [19:14]
            uint32_t evt1op2: 6;            // [25:20]
            uint32_t evt1op3: 6;            // [31:26]
        };
    } evtdet;      // 0x54

    union {
        uint32_t v;
        struct {
            uint32_t evt2tgtadd: 11;        // [10:0]
            uint32_t evt2opcomb: 2;         // [12:11]
            uint32_t reserved_13: 1;        // [13]
            uint32_t evt2op1: 6;            // [19:14]
            uint32_t evt2op2: 6;            // [25:20]
            uint32_t evt2op3: 6;            // [31:26]
        };
    } evtdis;      // 0x58

    union {
        uint32_t v;
        struct {
            uint32_t evt3tgtadd: 11;        // [10:0]
            uint32_t evt3opcomb: 2;         // [12:11]
            uint32_t reserved_13: 1;        // [13]
            uint32_t evt3op1: 6;            // [19:14]
            uint32_t evt3op2: 6;            // [25:20]
            uint32_t evt3op3: 6;            // [31:26]
        };
    } evtdsssdet;  // 0x5C

    union {
        uint32_t v;
        struct {
            uint32_t combpathsel: 4;        // [3:0]
            uint32_t gpstatus: 4;           // [7:4]
            uint32_t ofdmonly: 1;           // [8]
            uint32_t dsssonly: 1;           // [9]
            uint32_t rifsdeten: 1;          // [10]
            uint32_t radardeten: 1;         // [11]
            uint32_t agcfsmreset: 1;        // [12]
            uint32_t reserved_13_15: 3;     // [15:13]
            uint32_t htstfgainen: 1;        // [16]
            uint32_t reserved_17_31: 15;    // [31:17]
        };
    } cntl;        // 0x60

    union {
        uint32_t v;
        struct {
            uint32_t adcpowbiasqdb: 8;      // [7:0]
            uint32_t adcpowmindbv: 8;       // [15:8]
            uint32_t vpeakadcqdbv: 8;       // [23:16]
            uint32_t nbitadc: 4;            // [27:24]
            uint32_t reserved_28_31: 4;     // [31:28]
        };
    } dsp0;        // 0x64

    union {
        uint32_t v;
        struct {
            uint32_t inbdpow20pbiasqdb: 8;  // [7:0]
            uint32_t inbdpow20sbiasqdb: 8;  // [15:8]
            uint32_t inbdpow40sbiasqdb: 8;  // [23:16]
            uint32_t reserved_24_27: 4;     // [27:24]
            uint32_t inbdrnd: 2;            // [29:28]
            uint32_t reserved_30_31: 2;     // [31:30]
        };
    } dsp1;        // 0x68

    union {
        uint32_t v;
        struct {
            uint32_t fe20gain: 8;           // [7:0]
            uint32_t fe40gain: 8;           // [15:8]
            uint32_t inbdpowmindbm: 9;      // [24:16]
            uint32_t reserved_25_31: 7;     // [31:25]
        };
    } dsp2;        // 0x6C

    union {
        uint32_t v;
        struct {
            uint32_t ccadetrampup: 6;       // [5:0]
            uint32_t reserved_6_7: 2;       // [7:6]
            uint32_t ccadetrampdn: 6;       // [13:8]
            uint32_t reserved_14_15: 2;     // [15:14]
            uint32_t delccarampuptap: 3;    // [18:16]
            uint32_t reserved_19: 1;        // [19]
            uint32_t delccarampdntap: 3;    // [22:20]
            uint32_t reserved_23_31: 9;     // [31:23]
        };
    } cca0;        // 0x70

    union {
        uint32_t v;
        struct {
            uint32_t ccarisethrdbm: 8;      // [7:0]
            uint32_t satccaen: 1;           // [8]
            uint32_t disccaen: 1;           // [9]
            uint32_t reserved_10_11: 2;     // [11:10]
            uint32_t ccafallthrdbm: 8;      // [19:12]
            uint32_t inbdccapowmindbm: 9;   // [28:20]
            uint32_t reserved_29_31: 3;     // [31:29]
        };
    } cca1;        // 0x74

    union {
        uint32_t v;
        struct {
            uint32_t ccaenergyen: 3;        // [2:0]
            uint32_t reserved_3: 1;         // [3]
            uint32_t ccarampuden: 3;        // [6:4]
            uint32_t reserved_7: 1;         // [7]
            uint32_t ccacsen: 1;            // [8]
            uint32_t ccademod: 1;           // [9]
            uint32_t reserved_10_15: 6;     // [15:10]
            uint32_t ccaflag0ctrl: 4;       // [19:16]
            uint32_t ccaflag1ctrl: 4;       // [23:20]
            uint32_t ccaflag2ctrl: 4;       // [27:24]
            uint32_t ccaflag3ctrl: 4;       // [31:28]
        };
    } ccactrl;     // 0x78

    union {
        uint32_t v;
        struct {
            uint32_t rxstatecca20psel: 11;  // [10:0]
            uint32_t reserved_11_15: 5;     // [15:11]
            uint32_t rxstatecca20ssel: 11;  // [26:16]
            uint32_t reserved_27_31: 5;     // [31:27]
        };
    } ccastate0;   // 0x7C

    union {
        uint32_t v;
        struct {
            uint32_t rxstatecca40ssel: 11;  // [10:0]
            uint32_t reserved_11_31: 21;    // [31:11]
        };
    } ccastate1;   // 0x80

    uint32_t ccatimeout;                    // 0x84

    union {
        uint32_t v;
        struct {
            uint32_t nfvgathrl0: 4;         // [3:0]
            uint32_t nfvgathrl1: 4;         // [7:4]
            uint32_t nfvgathrl2: 4;         // [11:8]
            uint32_t nfvgathrm0: 4;         // [15:12]
            uint32_t nfvgathrm1: 4;         // [19:16]
            uint32_t nfvgathrm2: 4;         // [23:20]
            uint32_t reserved_24_31: 8;     // [31:24]
        };
    } snr0;        // 0x88

    union {
        uint32_t v;
        struct {
            uint32_t nfvgathrh0: 4;         // [3:0]
            uint32_t nfvgathrh1: 4;         // [7:4]
            uint32_t nfvgathrh2: 4;         // [11:8]
            uint32_t reserved_12_31: 20;    // [31:12]
        };
    } snr1;        // 0x8C

    union {
        uint32_t v;
        struct {
            uint32_t nfvall0db: 6;          // [5:0]
            uint32_t reserved_6_7: 2;       // [7:6]
            uint32_t nfvall1db: 6;          // [13:8]
            uint32_t reserved_14_15: 2;     // [15:14]
            uint32_t nfvall2db: 6;          // [21:16]
            uint32_t reserved_22_23: 2;     // [23:22]
            uint32_t nfvall3db: 6;          // [29:24]
            uint32_t reserved_30_31: 2;     // [31:30]
        };
    } snr2;        // 0x90

    union {
        uint32_t v;
        struct {
            uint32_t nfvalm0db: 6;          // [5:0]
            uint32_t reserved_6_7: 2;       // [7:6]
            uint32_t nfvalm1db: 6;          // [13:8]
            uint32_t reserved_14_15: 2;     // [15:14]
            uint32_t nfvalm2db: 6;          // [21:16]
            uint32_t reserved_22_23: 2;     // [23:22]
            uint32_t nfvalm3db: 6;          // [29:24]
            uint32_t reserved_30_31: 2;     // [31:30]
        };
    } snr3;        // 0x94

    union {
        uint32_t v;
        struct {
            uint32_t nfvalh0db: 6;          // [5:0]
            uint32_t reserved_6_7: 2;       // [7:6]
            uint32_t nfvalh1db: 6;          // [13:8]
            uint32_t reserved_14_15: 2;     // [15:14]
            uint32_t nfvalh2db: 6;          // [21:16]
            uint32_t reserved_22_23: 2;     // [23:22]
            uint32_t nfvalh3db: 6;          // [29:24]
            uint32_t reserved_30_31: 2;     // [31:30]
        };
    } snr4;        // 0x98

    union {
        uint32_t v;
        struct {
            uint32_t powsupthrdbm: 8;       // [7:0]
            uint32_t powinfthrdbm: 8;       // [15:8]
            uint32_t idpowgapupqdbm: 8;     // [23:16]
            uint32_t idpowgapdnqdbm: 8;     // [31:24]
        };
    } comp;        // 0x9C

    union {
        uint32_t v;
        struct {
            uint32_t fomsing: 8;            // [7:0]
            uint32_t fomsinggap: 4;         // [11:8]
            uint32_t reserved_12_15: 4;     // [15:12]
            uint32_t freqsing: 8;           // [23:16]
            uint32_t freqsinggap: 4;        // [27:24]
            uint32_t reserved_28_31: 4;     // [31:28]
        };
    } radar;       // 0xA0

    union {
        uint32_t v;
        struct {
            uint32_t pulseirqtimeout: 8;    // [7:0]
            uint32_t reserved_8_15: 8;      // [15:8]
            uint32_t pulseirqfifothr: 3;    // [18:16]
            uint32_t reserved_19_31: 13;    // [31:19]
        };
    } radartim;    // 0xA4

    union {
        uint32_t v;
        struct {
            uint32_t inbdpowdbvthr: 8;      // [7:0]
            uint32_t reserved_8_31: 24;     // [31:8]
        };
    } dsp3;        // 0xA8

    union {
        uint32_t v;
        struct {
            uint32_t adcpowsupdbmthr: 8;    // [7:0]
            uint32_t adcpowbiasqdbm: 8;     // [15:8]
            uint32_t adcpowmindbm: 8;       // [23:16]
            uint32_t reserved_24_31: 8;     // [31:24]
        };
    } dsp4;        // 0xAC
} hw_agc_t;

#define hw_agc ((volatile hw_agc_t *)AGC_BASE_ADDR)
