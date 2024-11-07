#pragma once

#include <stdint.h>
#include <sys/cdefs.h>


#define UART1_BASE_ADDR                 (0x0802100)
#define UART2_BASE_ADDR                 (0x0802200)

typedef struct {
    union {
        uint32_t v;
        struct __attribute__((aligned(4))) __packed {
            uint32_t tx_enable: 1;                  // [0]
            uint32_t rx_enable: 1;                  // [1]
            uint32_t irda: 1;                       // [2]
            uint32_t data_length: 2;                // [3:4]
            uint32_t parity_enable: 1;              // [5]
            uint32_t parity_odd_mode: 1;            // [6]
            uint32_t stop_length_2: 1;              // [7]
            uint32_t clk_divid: 13;                 // [8:20]
            uint32_t reserved_21_31: 11;            // [21:31]
        };
    } config;

    union {
        uint32_t v;
        struct __attribute__((aligned(4))) __packed {
            uint32_t tx_fifo_threshold: 8;          // [0:7]
            uint32_t rx_fifo_threshold: 8;          // [8:15]
            uint32_t rx_stop_detect_time: 2;        // [16:17]
            uint32_t reserved_18_31: 14;            // [18:31]
        };
    } fifo_config;

    union {
        uint32_t v;
        struct __attribute__((aligned(4))) __packed {
            uint32_t tx_fifo_count: 8;              // [0:7]
            uint32_t rx_fifo_count: 8;              // [8:15]
            uint32_t tx_full: 1;                    // [16]
            uint32_t tx_empty: 1;                   // [17]
            uint32_t rx_full: 1;                    // [18]
            uint32_t rx_empty: 1;                   // [19]
            uint32_t wr_ready: 1;                   // [20]
            uint32_t rd_ready: 1;                   // [21]
            uint32_t reserved_22_31: 10;            // [22:31]
        };
    } fifo_status;

    union {
        uint32_t v;
        struct __attribute__((aligned(4))) __packed {
            uint32_t tx: 8;                         // [0:7]
            uint32_t rx: 8;                         // [8:15]
            uint32_t reserved_16_31: 16;            // [16:31]
        };
    } fifo_data;

    union {
        uint32_t v;
        struct __attribute__((aligned(4))) __packed {
            uint32_t tx_need_write: 1;              // [0]
            uint32_t rx_need_read: 1;               // [1]
            uint32_t rx_overflow: 1;                // [2]
            uint32_t rx_parity_error: 1;            // [3]
            uint32_t rx_stop_error: 1;              // [4]
            uint32_t tx_stop_end: 1;                // [5]
            uint32_t rx_stop_end: 1;                // [6]
            uint32_t rxd_wakeup: 1;                 // [7]
            uint32_t reserved_8_31: 24;             // [8:31]
        };
    } irq_enable;

    union {
        uint32_t v;
        struct __attribute__((aligned(4))) __packed {
            uint32_t tx_need_write: 1;              // [0]
            uint32_t rx_need_read: 1;               // [1]
            uint32_t rx_overflow: 1;                // [2]
            uint32_t rx_parity_error: 1;            // [3]
            uint32_t rx_stop_error: 1;              // [4]
            uint32_t tx_stop_end: 1;                // [5]
            uint32_t rx_stop_end: 1;                // [6]
            uint32_t rxd_wakeup: 1;                 // [7]
            uint32_t reserved_8_31: 24;             // [8:31]
        };
    } irq_status;

    union {
        uint32_t v;
        struct __attribute__((aligned(4))) __packed {
            uint32_t low_cnt: 8;                    // [0:7]
            uint32_t high_cnt: 8;                   // [8:15]
            uint32_t enable: 1;                     // [16]
            uint32_t rts_polarity: 1;               // [17] only valid for uart1
            uint32_t cts_polarity: 1;               // [18] only valid for uart1
            uint32_t reserved_19_31: 13;            // [19:31]
        };
    } flow_config;

    union {
        uint32_t v;
        struct __attribute__((aligned(4))) __packed {
            uint32_t wake_count: 10;                // [0:9]
            uint32_t txd_wait_count: 10;            // [10:19]
            uint32_t rxd_wake_enable: 1;            // [20]
            uint32_t txd_wake_enable: 1;            // [21]
            uint32_t rxd_negedge_wake_enable: 1;    // [22]
            uint32_t reserved_23_31: 9;             // [23:31]
        };
    } wakeup_config;

} hw_uart_t;


#define hw_uart1                        ((volatile hw_uart_t *) UART1_BASE_ADDR)
#define hw_uart2                        ((volatile hw_uart_t *) UART2_BASE_ADDR)
