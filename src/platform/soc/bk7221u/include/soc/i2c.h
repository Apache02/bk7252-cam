#pragma once

#include "platform/soc.h"


#define I2C1_BASE_ADDR (0x00802300)
#define I2C2_BASE_ADDR (0x00802600)


// I2C1 layout
typedef volatile struct {
    union {
        uint32_t v;
        struct {
            uint32_t       ensmb: 1;           // [0]     enable the SMBus/I2C engine
            uint32_t       sta: 1;             // [1]     generate a START condition
            uint32_t       sto: 1;             // [2]     generate a STOP condition
            uint32_t       ack_tx: 1;          // [3]     ACK to send for the byte just received
            uint32_t       tx_mode: 1;         // [4]     1=transmit, 0=receive
            uint32_t       reserved_5: 1;      // [5]
            uint32_t       freq_div: 10;       // [15:6]  bus clock divider
            uint32_t       si: 1;              // [16]    serial interrupt flag; clear by writing 0
            const uint32_t ack_rx: 1;          // [17]    ACK bit of the last byte transferred, read-only
            const uint32_t ack_req: 1;         // [18]    set once an ACK/NACK decision is needed, read-only
            const uint32_t busy: 1;            // [19]    bus busy, read-only
            uint32_t       reserved_20_31: 12; // [31:20]
        };
    } config;

    union {
        uint32_t v;
        struct {
            uint32_t data: 8;           // [7:0]  next byte to send / last byte received
            uint32_t reserved_8_31: 24; // [31:8]
        };
    } data;
} hw_i2c1_t;


// I2C2 layout
typedef volatile struct {
    union {
        uint32_t v;
        struct {
            uint32_t idle_cr: 3;   // [2:0]   idle-bus timing config
            uint32_t scl_cr: 3;    // [5:3]   SCL timing config
            uint32_t freq_div: 10; // [15:6]  bus clock divider
            uint32_t slv_addr: 10; // [25:16] own slave address (slave mode)
            uint32_t smb_cs: 2;    // [27:26] SMBus clock select
            uint32_t toe: 1;       // [28]    timeout enable
            uint32_t fte: 1;       // [29]    FIFO/transfer enable
            uint32_t inh: 1;       // [30]    inhibit
            uint32_t ensmb: 1;     // [31]    enable the SMBus/I2C engine
        };
    } config;

    union {
        uint32_t v;
        struct {
            uint32_t       si: 1;              // [0]     serial interrupt flag; clear by writing 0
            uint32_t       scl_timeout: 1;     // [1]     SCL timeout occurred
            uint32_t       reserved_2: 1;      // [2]
            uint32_t       arb_lost: 1;        // [3]     arbitration lost
            const uint32_t rxfifo_empty: 1;    // [4]     RX FIFO empty, read-only
            const uint32_t txfifo_full: 1;     // [5]     TX FIFO full, read-only
            uint32_t       int_mode: 2;        // [7:6]   interrupt mode
            uint32_t       smbus_ack: 1;       // [8]     ACK bit
            uint32_t       smbus_stop: 1;      // [9]     generate/detect STOP
            uint32_t       smbus_sta: 1;       // [10]    generate/detect START
            const uint32_t addr_match: 1;      // [11]    address matched (slave mode), read-only
            const uint32_t ack_req: 1;         // [12]    ACK/NACK decision needed, read-only
            uint32_t       tx_mode: 1;         // [13]    1=transmit, 0=receive
            const uint32_t master: 1;          // [14]    master mode active, read-only
            const uint32_t busy: 1;            // [15]    bus busy, read-only
            uint32_t       reserved_16_31: 16; // [31:16]
        };
    } status;

    union {
        uint32_t v;
        struct {
            uint32_t data: 8;           // [7:0]  next byte to send / last byte received
            uint32_t reserved_8_31: 24; // [31:8]
        };
    } data;
} hw_i2c2_t;

#define hw_i2c1 ((volatile hw_i2c1_t *)I2C1_BASE_ADDR)
#define hw_i2c2 ((volatile hw_i2c2_t *)I2C2_BASE_ADDR)
