#pragma once

#include "platform/soc.h"


#define MAC_PL_BASE_ADDR (0xC0008000)


typedef volatile struct {
    uint32_t reserved_0x00_0x3c[16]; // [0x00:0x3C]

    union {
        uint32_t v;
        struct {
            uint32_t next_tbtt    : 16; // [15:0]  next target beacon transmission time
            uint32_t reserved_16_31 : 16; // [31:16]
        };
    } next_tbtt;              // 0x40

    uint32_t reserved_0x44;   // 0x44

    union {
        uint32_t v;
        struct {
            uint32_t wake_up_sw        : 1;  // [0]    software wake-up request
            uint32_t reserved_1_30     : 30; // [30:1]
            uint32_t wake_up_from_doze : 1;  // [31]   wake-up from doze status
        };
    } doze_cntrl_2;           // 0x48

    uint32_t reserved_0x4c;   // 0x4C

    union {
        uint32_t v;
        struct {
            uint32_t soft_reset    : 1;  // [0]    software reset; self-clearing
            uint32_t reserved_1_31 : 31; // [31:1]
        };
    } mac_cntrl_2;            // 0x50

    uint32_t reserved_0x54_0x68[6]; // [0x54:0x68]

    union {
        uint32_t v;
        struct {
            uint32_t imp_pri_tbtt         : 1; // [0]    impending primary TBTT interrupt
            uint32_t imp_sec_tbtt         : 1; // [1]    impending secondary TBTT interrupt
            uint32_t idle_interrupt       : 1; // [2]    idle interrupt
            uint32_t abs_gen_timers       : 1; // [3]    absolute general timers interrupt
            uint32_t reserved_4           : 1; // [4]
            uint32_t coex_event           : 1; // [5]    coexistence event interrupt
            uint32_t mac_phy_if_overflow  : 1; // [6]    MAC/PHY interface overflow
            uint32_t rx_dma_empty         : 1; // [7]    RX DMA empty
            uint32_t rx_fifo_over_flow    : 1; // [8]    RX FIFO overflow
            uint32_t olbc_ofdm            : 1; // [9]    OLBC OFDM detected
            uint32_t olbc_dsss            : 1; // [10]   OLBC DSSS detected
            uint32_t tim_set              : 1; // [11]   TIM set interrupt
            uint32_t pt_error             : 1; // [12]   PT error
            uint32_t ac0_tx_dma_dead      : 1; // [13]   AC0 TX DMA dead
            uint32_t ac1_tx_dma_dead      : 1; // [14]   AC1 TX DMA dead
            uint32_t ac2_tx_dma_dead      : 1; // [15]   AC2 TX DMA dead
            uint32_t ac3_tx_dma_dead      : 1; // [16]   AC3 TX DMA dead
            uint32_t bcn_tx_dma_dead      : 1; // [17]   beacon TX DMA dead
            uint32_t imp_pri_dtim         : 1; // [18]   impending primary DTIM
            uint32_t imp_sec_dtim         : 1; // [19]   impending secondary DTIM
            uint32_t hw_err               : 1; // [20]   hardware error
            uint32_t mac_phy_if_under_run : 1; // [21]   MAC/PHY interface underrun
            uint32_t phy_err              : 1; // [22]   PHY error
            uint32_t phy_rx_start         : 1; // [23]   PHY RX start
            uint32_t rx_header_dma_dead   : 1; // [24]   RX header DMA dead
            uint32_t rx_payload_dma_dead  : 1; // [25]   RX payload DMA dead
            uint32_t reserved_26_31       : 6; // [31:26]
        };
    } gen_int_status;         // 0x6C

    union {
        uint32_t v;
        struct {
            uint32_t imp_pri_tbtt         : 1; // [0]    acknowledge impending primary TBTT
            uint32_t imp_sec_tbtt         : 1; // [1]    acknowledge impending secondary TBTT
            uint32_t idle_interrupt       : 1; // [2]    acknowledge idle interrupt
            uint32_t reserved_3           : 1; // [3]
            uint32_t reserved_4           : 1; // [4]
            uint32_t coex_event           : 1; // [5]    acknowledge coexistence event
            uint32_t mac_phy_if_overflow  : 1; // [6]    acknowledge MAC/PHY interface overflow
            uint32_t rx_dma_empty         : 1; // [7]    acknowledge RX DMA empty
            uint32_t rx_fifo_over_flow    : 1; // [8]    acknowledge RX FIFO overflow
            uint32_t olbc_ofdm            : 1; // [9]    acknowledge OLBC OFDM
            uint32_t olbc_dsss            : 1; // [10]   acknowledge OLBC DSSS
            uint32_t tim_set              : 1; // [11]   acknowledge TIM set
            uint32_t pt_error             : 1; // [12]   acknowledge PT error
            uint32_t ac0_tx_dma_dead      : 1; // [13]   acknowledge AC0 TX DMA dead
            uint32_t ac1_tx_dma_dead      : 1; // [14]   acknowledge AC1 TX DMA dead
            uint32_t ac2_tx_dma_dead      : 1; // [15]   acknowledge AC2 TX DMA dead
            uint32_t ac3_tx_dma_dead      : 1; // [16]   acknowledge AC3 TX DMA dead
            uint32_t bcn_tx_dma_dead      : 1; // [17]   acknowledge beacon TX DMA dead
            uint32_t imp_pri_dtim         : 1; // [18]   acknowledge impending primary DTIM
            uint32_t imp_sec_dtim         : 1; // [19]   acknowledge impending secondary DTIM
            uint32_t hw_err               : 1; // [20]   acknowledge hardware error
            uint32_t mac_phy_if_under_run : 1; // [21]   acknowledge MAC/PHY interface underrun
            uint32_t phy_err              : 1; // [22]   acknowledge PHY error
            uint32_t phy_rx_start         : 1; // [23]   acknowledge PHY RX start
            uint32_t rx_header_dma_dead   : 1; // [24]   acknowledge RX header DMA dead
            uint32_t rx_payload_dma_dead  : 1; // [25]   acknowledge RX payload DMA dead
            uint32_t reserved_26_31       : 6; // [31:26]
        };
    } gen_int_ack;            // 0x70

    union {
        uint32_t v;
        struct {
            uint32_t imp_pri_tbtt         : 1; // [0]    enable impending primary TBTT interrupt
            uint32_t imp_sec_tbtt         : 1; // [1]    enable impending secondary TBTT interrupt
            uint32_t idle_interrupt       : 1; // [2]    enable idle interrupt
            uint32_t abs_gen_timers       : 1; // [3]    enable absolute general timers interrupt
            uint32_t reserved_4           : 1; // [4]
            uint32_t coex_event           : 1; // [5]    enable coexistence event interrupt
            uint32_t mac_phy_if_overflow  : 1; // [6]    enable MAC/PHY interface overflow interrupt
            uint32_t rx_dma_empty         : 1; // [7]    enable RX DMA empty interrupt
            uint32_t rx_fifo_over_flow    : 1; // [8]    enable RX FIFO overflow interrupt
            uint32_t olbc_ofdm            : 1; // [9]    enable OLBC OFDM interrupt
            uint32_t olbc_dsss            : 1; // [10]   enable OLBC DSSS interrupt
            uint32_t tim_set              : 1; // [11]   enable TIM set interrupt
            uint32_t pt_error             : 1; // [12]   enable PT error interrupt
            uint32_t ac0_tx_dma_dead      : 1; // [13]   enable AC0 TX DMA dead interrupt
            uint32_t ac1_tx_dma_dead      : 1; // [14]   enable AC1 TX DMA dead interrupt
            uint32_t ac2_tx_dma_dead      : 1; // [15]   enable AC2 TX DMA dead interrupt
            uint32_t ac3_tx_dma_dead      : 1; // [16]   enable AC3 TX DMA dead interrupt
            uint32_t bcn_tx_dma_dead      : 1; // [17]   enable beacon TX DMA dead interrupt
            uint32_t imp_pri_dtim         : 1; // [18]   enable impending primary DTIM interrupt
            uint32_t imp_sec_dtim         : 1; // [19]   enable impending secondary DTIM interrupt
            uint32_t hw_err               : 1; // [20]   enable hardware error interrupt
            uint32_t mac_phy_if_under_run : 1; // [21]   enable MAC/PHY interface underrun interrupt
            uint32_t phy_err              : 1; // [22]   enable PHY error interrupt
            uint32_t phy_rx_start         : 1; // [23]   enable PHY RX start interrupt
            uint32_t rx_header_dma_dead   : 1; // [24]   enable RX header DMA dead interrupt
            uint32_t rx_payload_dma_dead  : 1; // [25]   enable RX payload DMA dead interrupt
            uint32_t reserved_26_30       : 5; // [30:26]
            uint32_t master_gen_int_en    : 1; // [31]   master general interrupt enable
        };
    } gen_int_enable;         // 0x74

    union {
        uint32_t v;
        struct {
            uint32_t ac0_prot_trigger    : 1; // [0]    AC0 protection trigger
            uint32_t ac1_prot_trigger    : 1; // [1]    AC1 protection trigger
            uint32_t ac2_prot_trigger    : 1; // [2]    AC2 protection trigger
            uint32_t ac3_prot_trigger    : 1; // [3]    AC3 protection trigger
            uint32_t hcca_prot_trigger   : 1; // [4]    HCCA protection trigger
            uint32_t rd_prot_trigger     : 1; // [5]    RD protection trigger
            uint32_t ac0_tx_trigger      : 1; // [6]    AC0 TX trigger
            uint32_t ac1_tx_trigger      : 1; // [7]    AC1 TX trigger
            uint32_t ac2_tx_trigger      : 1; // [8]    AC2 TX trigger
            uint32_t ac3_tx_trigger      : 1; // [9]    AC3 TX trigger
            uint32_t bcn_tx_trigger      : 1; // [10]   beacon TX trigger
            uint32_t hcca_tx_trigger     : 1; // [11]   HCCA TX trigger
            uint32_t rd_tx_trigger       : 1; // [12]   RD TX trigger
            uint32_t txop_complete       : 1; // [13]   TXOP complete
            uint32_t timer_tx_trigger    : 1; // [14]   timer TX trigger
            uint32_t sec_user_tx_trigger : 1; // [15]   secondary user TX trigger
            uint32_t rx_trigger          : 1; // [16]   RX trigger
            uint32_t timer_rx_trigger    : 1; // [17]   timer RX trigger
            uint32_t ba_rx_trigger       : 1; // [18]   block-ack RX trigger
            uint32_t counter_rx_trigger  : 1; // [19]   counter RX trigger
            uint32_t ac0_bw_drop_trigger : 1; // [20]   AC0 bandwidth drop trigger
            uint32_t ac1_bw_drop_trigger : 1; // [21]   AC1 bandwidth drop trigger
            uint32_t ac2_bw_drop_trigger : 1; // [22]   AC2 bandwidth drop trigger
            uint32_t ac3_bw_drop_trigger : 1; // [23]   AC3 bandwidth drop trigger
            uint32_t ac0_tx_buf_trigger  : 1; // [24]   AC0 TX buffer trigger
            uint32_t ac1_tx_buf_trigger  : 1; // [25]   AC1 TX buffer trigger
            uint32_t ac2_tx_buf_trigger  : 1; // [26]   AC2 TX buffer trigger
            uint32_t ac3_tx_buf_trigger  : 1; // [27]   AC3 TX buffer trigger
            uint32_t bcn_tx_buf_trigger  : 1; // [28]   beacon TX buffer trigger
            uint32_t reserved_29_31      : 3; // [31:29]
        };
    } tx_rx_int_status;       // 0x78

    union {
        uint32_t v;
        struct {
            uint32_t ac0_prot_trigger    : 1; // [0]    acknowledge AC0 protection trigger
            uint32_t ac1_prot_trigger    : 1; // [1]    acknowledge AC1 protection trigger
            uint32_t ac2_prot_trigger    : 1; // [2]    acknowledge AC2 protection trigger
            uint32_t ac3_prot_trigger    : 1; // [3]    acknowledge AC3 protection trigger
            uint32_t hcca_prot_trigger   : 1; // [4]    acknowledge HCCA protection trigger
            uint32_t rd_prot_trigger     : 1; // [5]    acknowledge RD protection trigger
            uint32_t ac0_tx_trigger      : 1; // [6]    acknowledge AC0 TX trigger
            uint32_t ac1_tx_trigger      : 1; // [7]    acknowledge AC1 TX trigger
            uint32_t ac2_tx_trigger      : 1; // [8]    acknowledge AC2 TX trigger
            uint32_t ac3_tx_trigger      : 1; // [9]    acknowledge AC3 TX trigger
            uint32_t bcn_tx_trigger      : 1; // [10]   acknowledge beacon TX trigger
            uint32_t hcca_tx_trigger     : 1; // [11]   acknowledge HCCA TX trigger
            uint32_t rd_tx_trigger       : 1; // [12]   acknowledge RD TX trigger
            uint32_t txop_complete       : 1; // [13]   acknowledge TXOP complete
            uint32_t timer_tx_trigger    : 1; // [14]   acknowledge timer TX trigger
            uint32_t reserved_15         : 1; // [15]
            uint32_t rx_trigger          : 1; // [16]   acknowledge RX trigger
            uint32_t timer_rx_trigger    : 1; // [17]   acknowledge timer RX trigger
            uint32_t ba_rx_trigger       : 1; // [18]   acknowledge block-ack RX trigger
            uint32_t counter_rx_trigger  : 1; // [19]   acknowledge counter RX trigger
            uint32_t ac0_bw_drop_trigger : 1; // [20]   acknowledge AC0 bandwidth drop trigger
            uint32_t ac1_bw_drop_trigger : 1; // [21]   acknowledge AC1 bandwidth drop trigger
            uint32_t ac2_bw_drop_trigger : 1; // [22]   acknowledge AC2 bandwidth drop trigger
            uint32_t ac3_bw_drop_trigger : 1; // [23]   acknowledge AC3 bandwidth drop trigger
            uint32_t ac0_tx_buf_trigger  : 1; // [24]   acknowledge AC0 TX buffer trigger
            uint32_t ac1_tx_buf_trigger  : 1; // [25]   acknowledge AC1 TX buffer trigger
            uint32_t ac2_tx_buf_trigger  : 1; // [26]   acknowledge AC2 TX buffer trigger
            uint32_t ac3_tx_buf_trigger  : 1; // [27]   acknowledge AC3 TX buffer trigger
            uint32_t bcn_tx_buf_trigger  : 1; // [28]   acknowledge beacon TX buffer trigger
            uint32_t reserved_29_31      : 3; // [31:29]
        };
    } tx_rx_int_ack;          // 0x7C

    union {
        uint32_t v;
        struct {
            uint32_t ac0_prot_trigger    : 1; // [0]    enable AC0 protection trigger interrupt
            uint32_t ac1_prot_trigger    : 1; // [1]    enable AC1 protection trigger interrupt
            uint32_t ac2_prot_trigger    : 1; // [2]    enable AC2 protection trigger interrupt
            uint32_t ac3_prot_trigger    : 1; // [3]    enable AC3 protection trigger interrupt
            uint32_t hcca_prot_trigger   : 1; // [4]    enable HCCA protection trigger interrupt
            uint32_t rd_prot_trigger     : 1; // [5]    enable RD protection trigger interrupt
            uint32_t ac0_tx_trigger      : 1; // [6]    enable AC0 TX trigger interrupt
            uint32_t ac1_tx_trigger      : 1; // [7]    enable AC1 TX trigger interrupt
            uint32_t ac2_tx_trigger      : 1; // [8]    enable AC2 TX trigger interrupt
            uint32_t ac3_tx_trigger      : 1; // [9]    enable AC3 TX trigger interrupt
            uint32_t bcn_tx_trigger      : 1; // [10]   enable beacon TX trigger interrupt
            uint32_t hcca_tx_trigger     : 1; // [11]   enable HCCA TX trigger interrupt
            uint32_t rd_tx_trigger       : 1; // [12]   enable RD TX trigger interrupt
            uint32_t txop_complete       : 1; // [13]   enable TXOP complete interrupt
            uint32_t timer_tx_trigger    : 1; // [14]   enable timer TX trigger interrupt
            uint32_t sec_user_tx_trigger : 1; // [15]   enable secondary user TX trigger interrupt
            uint32_t rx_trigger          : 1; // [16]   enable RX trigger interrupt
            uint32_t timer_rx_trigger    : 1; // [17]   enable timer RX trigger interrupt
            uint32_t ba_rx_trigger       : 1; // [18]   enable block-ack RX trigger interrupt
            uint32_t counter_rx_trigger  : 1; // [19]   enable counter RX trigger interrupt
            uint32_t ac0_bw_drop_trigger : 1; // [20]   enable AC0 bandwidth drop trigger interrupt
            uint32_t ac1_bw_drop_trigger : 1; // [21]   enable AC1 bandwidth drop trigger interrupt
            uint32_t ac2_bw_drop_trigger : 1; // [22]   enable AC2 bandwidth drop trigger interrupt
            uint32_t ac3_bw_drop_trigger : 1; // [23]   enable AC3 bandwidth drop trigger interrupt
            uint32_t ac0_tx_buf_trigger  : 1; // [24]   enable AC0 TX buffer trigger interrupt
            uint32_t ac1_tx_buf_trigger  : 1; // [25]   enable AC1 TX buffer trigger interrupt
            uint32_t ac2_tx_buf_trigger  : 1; // [26]   enable AC2 TX buffer trigger interrupt
            uint32_t ac3_tx_buf_trigger  : 1; // [27]   enable AC3 TX buffer trigger interrupt
            uint32_t bcn_tx_buf_trigger  : 1; // [28]   enable beacon TX buffer trigger interrupt
            uint32_t reserved_29_30      : 2; // [30:29]
            uint32_t master_tx_rx_int_en : 1; // [31]   master TX/RX interrupt enable
        };
    } tx_rx_int_enable;       // 0x80

    union {
        uint32_t v;
        struct {
            uint32_t abs_timers0   : 1; // [0]    absolute timer 0 event; write 1 to set
            uint32_t abs_timers1   : 1; // [1]    absolute timer 1 event; write 1 to set
            uint32_t abs_timers2   : 1; // [2]    absolute timer 2 event; write 1 to set
            uint32_t abs_timers3   : 1; // [3]    absolute timer 3 event; write 1 to set
            uint32_t abs_timers4   : 1; // [4]    absolute timer 4 event; write 1 to set
            uint32_t abs_timers5   : 1; // [5]    absolute timer 5 event; write 1 to set
            uint32_t abs_timers6   : 1; // [6]    absolute timer 6 event; write 1 to set
            uint32_t abs_timers7   : 1; // [7]    absolute timer 7 event; write 1 to set
            uint32_t abs_timers8   : 1; // [8]    absolute timer 8 event; write 1 to set
            uint32_t abs_timers9   : 1; // [9]    absolute timer 9 event; write 1 to set
            uint32_t reserved_10_31 : 22; // [31:10]
        };
    } timers_int_event_set;   // 0x84

    union {
        uint32_t v;
        struct {
            uint32_t abs_timers0   : 1; // [0]    absolute timer 0 event; write 1 to clear
            uint32_t abs_timers1   : 1; // [1]    absolute timer 1 event; write 1 to clear
            uint32_t abs_timers2   : 1; // [2]    absolute timer 2 event; write 1 to clear
            uint32_t abs_timers3   : 1; // [3]    absolute timer 3 event; write 1 to clear
            uint32_t abs_timers4   : 1; // [4]    absolute timer 4 event; write 1 to clear
            uint32_t abs_timers5   : 1; // [5]    absolute timer 5 event; write 1 to clear
            uint32_t abs_timers6   : 1; // [6]    absolute timer 6 event; write 1 to clear
            uint32_t abs_timers7   : 1; // [7]    absolute timer 7 event; write 1 to clear
            uint32_t abs_timers8   : 1; // [8]    absolute timer 8 event; write 1 to clear
            uint32_t abs_timers9   : 1; // [9]    absolute timer 9 event; write 1 to clear
            uint32_t reserved_10_31 : 22; // [31:10]
        };
    } timers_int_event_clear; // 0x88

    union {
        uint32_t v;
        struct {
            uint32_t mask_abs_timers0   : 1; // [0]    unmask absolute timer 0 interrupt
            uint32_t mask_abs_timers1   : 1; // [1]    unmask absolute timer 1 interrupt
            uint32_t mask_abs_timers2   : 1; // [2]    unmask absolute timer 2 interrupt
            uint32_t mask_abs_timers3   : 1; // [3]    unmask absolute timer 3 interrupt
            uint32_t mask_abs_timers4   : 1; // [4]    unmask absolute timer 4 interrupt
            uint32_t mask_abs_timers5   : 1; // [5]    unmask absolute timer 5 interrupt
            uint32_t mask_abs_timers6   : 1; // [6]    unmask absolute timer 6 interrupt
            uint32_t mask_abs_timers7   : 1; // [7]    unmask absolute timer 7 interrupt
            uint32_t mask_abs_timers8   : 1; // [8]    unmask absolute timer 8 interrupt
            uint32_t mask_abs_timers9   : 1; // [9]    unmask absolute timer 9 interrupt
            uint32_t reserved_10_31     : 22; // [31:10]
        };
    } timers_int_un_mask;     // 0x8C

    uint32_t reserved_0x90_0xa0[5]; // [0x90:0xA0]

    uint32_t tsf_lo;          // 0xA4  TSF timer low 32 bits
    uint32_t tsf_hi;          // 0xA8  TSF timer high 32 bits

    uint32_t reserved_0xac_0x15c[45]; // [0xAC:0x15C]

    union {
        uint32_t v;
        struct {
            uint32_t ppdu_length      : 20; // [19:0]   PPDU length in bytes
            uint32_t reserved_20_21   : 2;  // [21:20]
            uint32_t ppdu_bw          : 2;  // [23:22]  PPDU bandwidth; 0=20MHz 1=40MHz 2=80MHz
            uint32_t ppdu_pre_type    : 3;  // [26:24]  PPDU preamble type
            uint32_t ppdu_short_gi    : 1;  // [27]     short guard interval
            uint32_t ppdu_num_extn_ss : 2;  // [29:28]  number of extension spatial streams
            uint32_t ppdu_stbc        : 2;  // [31:30]  space-time block coding streams
        };
    } time_on_air_param_1;    // 0x160

    union {
        uint32_t v;
        struct {
            uint32_t ppdu_mcs_index : 7;  // [6:0]   MCS index for time-on-air calculation
            uint32_t reserved_7_31  : 25; // [31:7]
        };
    } time_on_air_param_2;    // 0x164

    union {
        uint32_t v;
        struct {
            uint32_t time_on_air       : 16; // [15:0]  computed time-on-air in microseconds
            uint32_t reserved_16_29    : 14; // [29:16]
            uint32_t time_on_air_valid : 1;  // [30]    1 = time_on_air value is valid
            uint32_t compute_duration  : 1;  // [31]    write 1 to trigger time-on-air computation
        };
    } time_on_air_value;      // 0x168

    uint32_t reserved_0x16c_0x17c[5]; // [0x16C:0x17C]

    union {
        uint32_t v;
        struct {
            uint32_t tx_bcn_new_tail     : 1; // [0]    new BCN TX tail descriptor; write 1 to set
            uint32_t tx_ac0_new_tail     : 1; // [1]    new AC0 TX tail descriptor; write 1 to set
            uint32_t tx_ac1_new_tail     : 1; // [2]    new AC1 TX tail descriptor; write 1 to set
            uint32_t tx_ac2_new_tail     : 1; // [3]    new AC2 TX tail descriptor; write 1 to set
            uint32_t tx_ac3_new_tail     : 1; // [4]    new AC3 TX tail descriptor; write 1 to set
            uint32_t reserved_5_7        : 3; // [7:5]
            uint32_t tx_bcn_new_head     : 1; // [8]    new BCN TX head descriptor; write 1 to set
            uint32_t tx_ac0_new_head     : 1; // [9]    new AC0 TX head descriptor; write 1 to set
            uint32_t tx_ac1_new_head     : 1; // [10]   new AC1 TX head descriptor; write 1 to set
            uint32_t tx_ac2_new_head     : 1; // [11]   new AC2 TX head descriptor; write 1 to set
            uint32_t tx_ac3_new_head     : 1; // [12]   new AC3 TX head descriptor; write 1 to set
            uint32_t reserved_13_14      : 2; // [14:13]
            uint32_t halt_bcn_after_txop : 1; // [15]   halt beacon after TXOP; write 1 to set
            uint32_t halt_ac0_after_txop : 1; // [16]   halt AC0 after TXOP; write 1 to set
            uint32_t halt_ac1_after_txop : 1; // [17]   halt AC1 after TXOP; write 1 to set
            uint32_t halt_ac2_after_txop : 1; // [18]   halt AC2 after TXOP; write 1 to set
            uint32_t halt_ac3_after_txop : 1; // [19]   halt AC3 after TXOP; write 1 to set
            uint32_t reserved_20_23      : 4; // [23:20]
            uint32_t rx_header_new_tail  : 1; // [24]   new RX header tail descriptor; write 1 to set
            uint32_t rx_payload_new_tail : 1; // [25]   new RX payload tail descriptor; write 1 to set
            uint32_t rx_header_new_head  : 1; // [26]   new RX header head descriptor; write 1 to set
            uint32_t rx_payload_new_head : 1; // [27]   new RX payload head descriptor; write 1 to set
            uint32_t reserved_28_31      : 4; // [31:28]
        };
    } dma_cntrl_set;          // 0x180

    union {
        uint32_t v;
        struct {
            uint32_t tx_bcn_new_tail     : 1; // [0]    new BCN TX tail descriptor; write 1 to clear
            uint32_t tx_ac0_new_tail     : 1; // [1]    new AC0 TX tail descriptor; write 1 to clear
            uint32_t tx_ac1_new_tail     : 1; // [2]    new AC1 TX tail descriptor; write 1 to clear
            uint32_t tx_ac2_new_tail     : 1; // [3]    new AC2 TX tail descriptor; write 1 to clear
            uint32_t tx_ac3_new_tail     : 1; // [4]    new AC3 TX tail descriptor; write 1 to clear
            uint32_t reserved_5_7        : 3; // [7:5]
            uint32_t tx_bcn_new_head     : 1; // [8]    new BCN TX head descriptor; write 1 to clear
            uint32_t tx_ac0_new_head     : 1; // [9]    new AC0 TX head descriptor; write 1 to clear
            uint32_t tx_ac1_new_head     : 1; // [10]   new AC1 TX head descriptor; write 1 to clear
            uint32_t tx_ac2_new_head     : 1; // [11]   new AC2 TX head descriptor; write 1 to clear
            uint32_t tx_ac3_new_head     : 1; // [12]   new AC3 TX head descriptor; write 1 to clear
            uint32_t reserved_13_14      : 2; // [14:13]
            uint32_t halt_bcn_after_txop : 1; // [15]   halt beacon after TXOP; write 1 to clear
            uint32_t halt_ac0_after_txop : 1; // [16]   halt AC0 after TXOP; write 1 to clear
            uint32_t halt_ac1_after_txop : 1; // [17]   halt AC1 after TXOP; write 1 to clear
            uint32_t halt_ac2_after_txop : 1; // [18]   halt AC2 after TXOP; write 1 to clear
            uint32_t halt_ac3_after_txop : 1; // [19]   halt AC3 after TXOP; write 1 to clear
            uint32_t reserved_20_23      : 4; // [23:20]
            uint32_t rx_header_new_tail  : 1; // [24]   new RX header tail descriptor; write 1 to clear
            uint32_t rx_payload_new_tail : 1; // [25]   new RX payload tail descriptor; write 1 to clear
            uint32_t rx_header_new_head  : 1; // [26]   new RX header head descriptor; write 1 to clear
            uint32_t rx_payload_new_head : 1; // [27]   new RX payload head descriptor; write 1 to clear
            uint32_t reserved_28_31      : 4; // [31:28]
        };
    } dma_cntrl_clear;        // 0x184

    union {
        uint32_t v;
        struct {
            uint32_t tx_bcn_state     : 2; // [1:0]   beacon TX DMA state
            uint32_t reserved_2_3     : 2; // [3:2]
            uint32_t tx_ac0_state     : 2; // [5:4]   AC0 TX DMA state
            uint32_t reserved_6_7     : 2; // [7:6]
            uint32_t tx_ac1_state     : 2; // [9:8]   AC1 TX DMA state
            uint32_t reserved_10_11   : 2; // [11:10]
            uint32_t tx_ac2_state     : 2; // [13:12] AC2 TX DMA state
            uint32_t reserved_14_15   : 2; // [15:14]
            uint32_t tx_ac3_state     : 2; // [17:16] AC3 TX DMA state
            uint32_t reserved_18_23   : 6; // [23:18]
            uint32_t rx_header_state  : 2; // [25:24] RX header DMA state
            uint32_t reserved_26_27   : 2; // [27:26]
            uint32_t rx_payload_state : 2; // [29:28] RX payload DMA state
            uint32_t reserved_30_31   : 2; // [31:30]
        };
    } dma_status_1;           // 0x188

    union {
        uint32_t v;
        struct {
            uint32_t tx_bcn_len_mismatch  : 1; // [0]    beacon TX length mismatch error
            uint32_t tx_ac0_len_mismatch  : 1; // [1]    AC0 TX length mismatch error
            uint32_t tx_ac1_len_mismatch  : 1; // [2]    AC1 TX length mismatch error
            uint32_t tx_ac2_len_mismatch  : 1; // [3]    AC2 TX length mismatch error
            uint32_t tx_ac3_len_mismatch  : 1; // [4]    AC3 TX length mismatch error
            uint32_t tx_bcn_u_pattern_err : 1; // [5]    beacon TX unexpected pattern error
            uint32_t tx_ac0_u_pattern_err : 1; // [6]    AC0 TX unexpected pattern error
            uint32_t tx_ac1_u_pattern_err : 1; // [7]    AC1 TX unexpected pattern error
            uint32_t tx_ac2_u_pattern_err : 1; // [8]    AC2 TX unexpected pattern error
            uint32_t tx_ac3_u_pattern_err : 1; // [9]    AC3 TX unexpected pattern error
            uint32_t tx_bcn_next_ptr_err  : 1; // [10]   beacon TX next-pointer error
            uint32_t tx_ac0_next_ptr_err  : 1; // [11]   AC0 TX next-pointer error
            uint32_t tx_ac1_next_ptr_err  : 1; // [12]   AC1 TX next-pointer error
            uint32_t tx_ac2_next_ptr_err  : 1; // [13]   AC2 TX next-pointer error
            uint32_t tx_ac3_next_ptr_err  : 1; // [14]   AC3 TX next-pointer error
            uint32_t tx_bcn_pt_addr_err   : 1; // [15]   beacon PT address error
            uint32_t tx_ac0_pt_addr_err   : 1; // [16]   AC0 PT address error
            uint32_t tx_ac1_pt_addr_err   : 1; // [17]   AC1 PT address error
            uint32_t tx_ac2_pt_addr_err   : 1; // [18]   AC2 PT address error
            uint32_t tx_ac3_pt_addr_err   : 1; // [19]   AC3 PT address error
            uint32_t tx_bcn_bus_err       : 1; // [20]   beacon TX bus error
            uint32_t tx_ac0_bus_err       : 1; // [21]   AC0 TX bus error
            uint32_t tx_ac1_bus_err       : 1; // [22]   AC1 TX bus error
            uint32_t tx_ac2_bus_err       : 1; // [23]   AC2 TX bus error
            uint32_t tx_ac3_bus_err       : 1; // [24]   AC3 TX bus error
            uint32_t tx_bcn_new_head_err  : 1; // [25]   beacon TX new-head error
            uint32_t tx_ac0_new_head_err  : 1; // [26]   AC0 TX new-head error
            uint32_t tx_ac1_new_head_err  : 1; // [27]   AC1 TX new-head error
            uint32_t tx_ac2_new_head_err  : 1; // [28]   AC2 TX new-head error
            uint32_t tx_ac3_new_head_err  : 1; // [29]   AC3 TX new-head error
            uint32_t reserved_30_31       : 2; // [31:30]
        };
    } dma_status_2;           // 0x18C

    union {
        uint32_t v;
        struct {
            uint32_t rx_hdr_u_pattern_err  : 1; // [0]    RX header unexpected pattern error
            uint32_t rx_pay_u_pattern_err  : 1; // [1]    RX payload unexpected pattern error
            uint32_t rx_hdr_next_ptr_err   : 1; // [2]    RX header next-pointer error
            uint32_t rx_pay_next_ptr_err   : 1; // [3]    RX payload next-pointer error
            uint32_t rx_hdr_bus_err        : 1; // [4]    RX header bus error
            uint32_t rx_pay_bus_err        : 1; // [5]    RX payload bus error
            uint32_t rx_hdr_new_head_err   : 1; // [6]    RX header new-head error
            uint32_t rx_pay_new_head_err   : 1; // [7]    RX payload new-head error
            uint32_t reserved_8_31         : 24; // [31:8]
        };
    } dma_status_3;           // 0x190

    union {
        uint32_t v;
        struct {
            uint32_t tx_bcn_startup       : 1; // [0]    beacon TX startup state
            uint32_t tx_ac0_startup       : 1; // [1]    AC0 TX startup state
            uint32_t tx_ac1_startup       : 1; // [2]    AC1 TX startup state
            uint32_t tx_ac2_startup       : 1; // [3]    AC2 TX startup state
            uint32_t tx_ac3_startup       : 1; // [4]    AC3 TX startup state
            uint32_t tx_bcn_end_q         : 1; // [5]    beacon TX end-of-queue status
            uint32_t tx_ac0_end_q         : 1; // [6]    AC0 TX end-of-queue status
            uint32_t tx_ac1_end_q         : 1; // [7]    AC1 TX end-of-queue status
            uint32_t tx_ac2_end_q         : 1; // [8]    AC2 TX end-of-queue status
            uint32_t tx_ac3_end_q         : 1; // [9]    AC3 TX end-of-queue status
            uint32_t tx_bcn_halt_after_txop : 1; // [10] beacon TX halted after TXOP
            uint32_t tx_ac0_halt_after_txop : 1; // [11] AC0 TX halted after TXOP
            uint32_t tx_ac1_halt_after_txop : 1; // [12] AC1 TX halted after TXOP
            uint32_t tx_ac2_halt_after_txop : 1; // [13] AC2 TX halted after TXOP
            uint32_t tx_ac3_halt_after_txop : 1; // [14] AC3 TX halted after TXOP
            uint32_t reserved_15_31         : 17; // [31:15]
        };
    } dma_status_4;           // 0x194

    uint32_t tx_bcn_head_ptr; // 0x198  beacon TX descriptor list head pointer
    uint32_t tx_ac0_head_ptr; // 0x19C  AC0 TX descriptor list head pointer
    uint32_t tx_ac1_head_ptr; // 0x1A0  AC1 TX descriptor list head pointer
    uint32_t tx_ac2_head_ptr; // 0x1A4  AC2 TX descriptor list head pointer
    uint32_t tx_ac3_head_ptr; // 0x1A8  AC3 TX descriptor list head pointer

    union {
        uint32_t v;
        struct {
            uint32_t pt_entry_size  : 6; // [5:0]   policy-table entry size in 32-bit words
            uint32_t dma_thd_size   : 6; // [11:6]  TX header descriptor size in 32-bit words
            uint32_t dma_tbd_size   : 6; // [17:12] TX buffer descriptor size in 32-bit words
            uint32_t dma_rhd_size   : 6; // [23:18] RX header descriptor size in 32-bit words
            uint32_t dma_rbd_size   : 6; // [29:24] RX buffer descriptor size in 32-bit words
            uint32_t reserved_30_31 : 2; // [31:30]
        };
    } tx_struct_sizes;        // 0x1AC

    uint32_t reserved_0x1b0_0x1b4[2]; // [0x1B0:0x1B4]

    union {
        uint32_t v;
        struct {
            uint32_t rx_header_head_ptr_valid : 1;  // [0]    1 = rxHeaderHeadPtr is valid
            uint32_t reserved_1               : 1;  // [1]
            uint32_t rx_header_head_ptr       : 30; // [31:2] RX header descriptor list head pointer (word-aligned)
        };
    } rx_header_head_ptr;     // 0x1B8

    union {
        uint32_t v;
        struct {
            uint32_t rx_payload_head_ptr_valid : 1;  // [0]    1 = rxPayloadHeadPtr is valid
            uint32_t reserved_1                : 1;  // [1]
            uint32_t rx_payload_head_ptr       : 30; // [31:2] RX payload descriptor list head pointer (word-aligned)
        };
    } rx_payload_head_ptr;    // 0x1BC

    union {
        uint32_t v;
        struct {
            uint32_t tx_fifo_threshold : 8;  // [7:0]   TX FIFO threshold in 32-bit words
            uint32_t reserved_8_15     : 8;  // [15:8]
            uint32_t rx_fifo_threshold : 8;  // [23:16] RX FIFO threshold in 32-bit words
            uint32_t reserved_24_31    : 8;  // [31:24]
        };
    } dma_threshold;          // 0x1C0

    uint32_t reserved_0x1c4_0x20c[19]; // [0x1C4:0x20C]

    union {
        uint32_t v;
        struct {
            uint32_t ac0_has_data  : 1; // [0]    AC0 has data queued; write 1 to set
            uint32_t ac1_has_data  : 1; // [1]    AC1 has data queued; write 1 to set
            uint32_t ac2_has_data  : 1; // [2]    AC2 has data queued; write 1 to set
            uint32_t ac3_has_data  : 1; // [3]    AC3 has data queued; write 1 to set
            uint32_t reserved_4_31 : 28; // [31:4]
        };
    } edca_ac_has_data_set;   // 0x210

    union {
        uint32_t v;
        struct {
            uint32_t ac0_has_data  : 1; // [0]    AC0 has data queued; write 1 to clear
            uint32_t ac1_has_data  : 1; // [1]    AC1 has data queued; write 1 to clear
            uint32_t ac2_has_data  : 1; // [2]    AC2 has data queued; write 1 to clear
            uint32_t ac3_has_data  : 1; // [3]    AC3 has data queued; write 1 to clear
            uint32_t reserved_4_31 : 28; // [31:4]
        };
    } edca_ac_has_data_clear; // 0x214

    uint32_t reserved_0x218_0x224[4]; // [0x218:0x224]

    union {
        uint32_t v;
        struct {
            uint32_t ac0_mot : 16; // [15:0]  AC0 medium occupancy timer
            uint32_t ac1_mot : 16; // [31:16] AC1 medium occupancy timer
        };
    } mot_1;                  // 0x228

    union {
        uint32_t v;
        struct {
            uint32_t ac2_mot : 16; // [15:0]  AC2 medium occupancy timer
            uint32_t ac3_mot : 16; // [31:16] AC3 medium occupancy timer
        };
    } mot_2;                  // 0x22C

    union {
        uint32_t v;
        struct {
            uint32_t hcca_qap_mot : 16; // [15:0]  HCCA/QAP medium occupancy timer
            uint32_t misc_mot     : 16; // [31:16] miscellaneous medium occupancy timer
        };
    } mot_3;                  // 0x230

    uint32_t reserved_0x234_0x32c[63]; // [0x234:0x32C]

    union {
        uint32_t v;
        struct {
            uint32_t tx_bw_after_drop : 2; // [1:0]   TX bandwidth after bandwidth-drop event
            uint32_t reserved_2_31    : 30; // [31:2]
        };
    } tx_bw_drop_info;        // 0x330

    uint32_t reserved_0x334_0x360[12]; // [0x334:0x360]

    // SEC_USERS_TX_INT_EVENT_SET, _CLEAR, _UN_MASK: present only when the
    // MU-MIMO secondary-users TX path (RW_MUMIMO_TX_EN) is compiled in.
    union {
        uint32_t v;
        struct {
            uint32_t sec_u1_ac0_tx_trigger      : 1; // [0]    sec-user 1 AC0 TX trigger; write 1 to set
            uint32_t sec_u1_ac1_tx_trigger      : 1; // [1]    sec-user 1 AC1 TX trigger; write 1 to set
            uint32_t sec_u1_ac2_tx_trigger      : 1; // [2]    sec-user 1 AC2 TX trigger; write 1 to set
            uint32_t sec_u1_ac3_tx_trigger      : 1; // [3]    sec-user 1 AC3 TX trigger; write 1 to set
            uint32_t sec_u1_ac0_tx_buf_trigger  : 1; // [4]    sec-user 1 AC0 TX buffer trigger; write 1 to set
            uint32_t sec_u1_ac1_tx_buf_trigger  : 1; // [5]    sec-user 1 AC1 TX buffer trigger; write 1 to set
            uint32_t sec_u1_ac2_tx_buf_trigger  : 1; // [6]    sec-user 1 AC2 TX buffer trigger; write 1 to set
            uint32_t sec_u1_ac3_tx_buf_trigger  : 1; // [7]    sec-user 1 AC3 TX buffer trigger; write 1 to set
            uint32_t sec_u2_ac0_tx_trigger      : 1; // [8]    sec-user 2 AC0 TX trigger; write 1 to set
            uint32_t sec_u2_ac1_tx_trigger      : 1; // [9]    sec-user 2 AC1 TX trigger; write 1 to set
            uint32_t sec_u2_ac2_tx_trigger      : 1; // [10]   sec-user 2 AC2 TX trigger; write 1 to set
            uint32_t sec_u2_ac3_tx_trigger      : 1; // [11]   sec-user 2 AC3 TX trigger; write 1 to set
            uint32_t sec_u2_ac0_tx_buf_trigger  : 1; // [12]   sec-user 2 AC0 TX buffer trigger; write 1 to set
            uint32_t sec_u2_ac1_tx_buf_trigger  : 1; // [13]   sec-user 2 AC1 TX buffer trigger; write 1 to set
            uint32_t sec_u2_ac2_tx_buf_trigger  : 1; // [14]   sec-user 2 AC2 TX buffer trigger; write 1 to set
            uint32_t sec_u2_ac3_tx_buf_trigger  : 1; // [15]   sec-user 2 AC3 TX buffer trigger; write 1 to set
            uint32_t sec_u3_ac0_tx_trigger      : 1; // [16]   sec-user 3 AC0 TX trigger; write 1 to set
            uint32_t sec_u3_ac1_tx_trigger      : 1; // [17]   sec-user 3 AC1 TX trigger; write 1 to set
            uint32_t sec_u3_ac2_tx_trigger      : 1; // [18]   sec-user 3 AC2 TX trigger; write 1 to set
            uint32_t sec_u3_ac3_tx_trigger      : 1; // [19]   sec-user 3 AC3 TX trigger; write 1 to set
            uint32_t sec_u3_ac0_tx_buf_trigger  : 1; // [20]   sec-user 3 AC0 TX buffer trigger; write 1 to set
            uint32_t sec_u3_ac1_tx_buf_trigger  : 1; // [21]   sec-user 3 AC1 TX buffer trigger; write 1 to set
            uint32_t sec_u3_ac2_tx_buf_trigger  : 1; // [22]   sec-user 3 AC2 TX buffer trigger; write 1 to set
            uint32_t sec_u3_ac3_tx_buf_trigger  : 1; // [23]   sec-user 3 AC3 TX buffer trigger; write 1 to set
            uint32_t reserved_24_31             : 8; // [31:24]
        };
    } sec_users_tx_int_event_set;   // 0x364

    union {
        uint32_t v;
        struct {
            uint32_t sec_u1_ac0_tx_trigger      : 1; // [0]    sec-user 1 AC0 TX trigger; write 1 to clear
            uint32_t sec_u1_ac1_tx_trigger      : 1; // [1]    sec-user 1 AC1 TX trigger; write 1 to clear
            uint32_t sec_u1_ac2_tx_trigger      : 1; // [2]    sec-user 1 AC2 TX trigger; write 1 to clear
            uint32_t sec_u1_ac3_tx_trigger      : 1; // [3]    sec-user 1 AC3 TX trigger; write 1 to clear
            uint32_t sec_u1_ac0_tx_buf_trigger  : 1; // [4]    sec-user 1 AC0 TX buffer trigger; write 1 to clear
            uint32_t sec_u1_ac1_tx_buf_trigger  : 1; // [5]    sec-user 1 AC1 TX buffer trigger; write 1 to clear
            uint32_t sec_u1_ac2_tx_buf_trigger  : 1; // [6]    sec-user 1 AC2 TX buffer trigger; write 1 to clear
            uint32_t sec_u1_ac3_tx_buf_trigger  : 1; // [7]    sec-user 1 AC3 TX buffer trigger; write 1 to clear
            uint32_t sec_u2_ac0_tx_trigger      : 1; // [8]    sec-user 2 AC0 TX trigger; write 1 to clear
            uint32_t sec_u2_ac1_tx_trigger      : 1; // [9]    sec-user 2 AC1 TX trigger; write 1 to clear
            uint32_t sec_u2_ac2_tx_trigger      : 1; // [10]   sec-user 2 AC2 TX trigger; write 1 to clear
            uint32_t sec_u2_ac3_tx_trigger      : 1; // [11]   sec-user 2 AC3 TX trigger; write 1 to clear
            uint32_t sec_u2_ac0_tx_buf_trigger  : 1; // [12]   sec-user 2 AC0 TX buffer trigger; write 1 to clear
            uint32_t sec_u2_ac1_tx_buf_trigger  : 1; // [13]   sec-user 2 AC1 TX buffer trigger; write 1 to clear
            uint32_t sec_u2_ac2_tx_buf_trigger  : 1; // [14]   sec-user 2 AC2 TX buffer trigger; write 1 to clear
            uint32_t sec_u2_ac3_tx_buf_trigger  : 1; // [15]   sec-user 2 AC3 TX buffer trigger; write 1 to clear
            uint32_t sec_u3_ac0_tx_trigger      : 1; // [16]   sec-user 3 AC0 TX trigger; write 1 to clear
            uint32_t sec_u3_ac1_tx_trigger      : 1; // [17]   sec-user 3 AC1 TX trigger; write 1 to clear
            uint32_t sec_u3_ac2_tx_trigger      : 1; // [18]   sec-user 3 AC2 TX trigger; write 1 to clear
            uint32_t sec_u3_ac3_tx_trigger      : 1; // [19]   sec-user 3 AC3 TX trigger; write 1 to clear
            uint32_t sec_u3_ac0_tx_buf_trigger  : 1; // [20]   sec-user 3 AC0 TX buffer trigger; write 1 to clear
            uint32_t sec_u3_ac1_tx_buf_trigger  : 1; // [21]   sec-user 3 AC1 TX buffer trigger; write 1 to clear
            uint32_t sec_u3_ac2_tx_buf_trigger  : 1; // [22]   sec-user 3 AC2 TX buffer trigger; write 1 to clear
            uint32_t sec_u3_ac3_tx_buf_trigger  : 1; // [23]   sec-user 3 AC3 TX buffer trigger; write 1 to clear
            uint32_t reserved_24_31             : 8; // [31:24]
        };
    } sec_users_tx_int_event_clear; // 0x368

    union {
        uint32_t v;
        struct {
            uint32_t mask_sec_u1_ac0_tx_trigger      : 1; // [0]    unmask sec-user 1 AC0 TX trigger interrupt
            uint32_t mask_sec_u1_ac1_tx_trigger      : 1; // [1]    unmask sec-user 1 AC1 TX trigger interrupt
            uint32_t mask_sec_u1_ac2_tx_trigger      : 1; // [2]    unmask sec-user 1 AC2 TX trigger interrupt
            uint32_t mask_sec_u1_ac3_tx_trigger      : 1; // [3]    unmask sec-user 1 AC3 TX trigger interrupt
            uint32_t mask_sec_u1_ac0_tx_buf_trigger  : 1; // [4]    unmask sec-user 1 AC0 TX buffer trigger interrupt
            uint32_t mask_sec_u1_ac1_tx_buf_trigger  : 1; // [5]    unmask sec-user 1 AC1 TX buffer trigger interrupt
            uint32_t mask_sec_u1_ac2_tx_buf_trigger  : 1; // [6]    unmask sec-user 1 AC2 TX buffer trigger interrupt
            uint32_t mask_sec_u1_ac3_tx_buf_trigger  : 1; // [7]    unmask sec-user 1 AC3 TX buffer trigger interrupt
            uint32_t mask_sec_u2_ac0_tx_trigger      : 1; // [8]    unmask sec-user 2 AC0 TX trigger interrupt
            uint32_t mask_sec_u2_ac1_tx_trigger      : 1; // [9]    unmask sec-user 2 AC1 TX trigger interrupt
            uint32_t mask_sec_u2_ac2_tx_trigger      : 1; // [10]   unmask sec-user 2 AC2 TX trigger interrupt
            uint32_t mask_sec_u2_ac3_tx_trigger      : 1; // [11]   unmask sec-user 2 AC3 TX trigger interrupt
            uint32_t mask_sec_u2_ac0_tx_buf_trigger  : 1; // [12]   unmask sec-user 2 AC0 TX buffer trigger interrupt
            uint32_t mask_sec_u2_ac1_tx_buf_trigger  : 1; // [13]   unmask sec-user 2 AC1 TX buffer trigger interrupt
            uint32_t mask_sec_u2_ac2_tx_buf_trigger  : 1; // [14]   unmask sec-user 2 AC2 TX buffer trigger interrupt
            uint32_t mask_sec_u2_ac3_tx_buf_trigger  : 1; // [15]   unmask sec-user 2 AC3 TX buffer trigger interrupt
            uint32_t mask_sec_u3_ac0_tx_trigger      : 1; // [16]   unmask sec-user 3 AC0 TX trigger interrupt
            uint32_t mask_sec_u3_ac1_tx_trigger      : 1; // [17]   unmask sec-user 3 AC1 TX trigger interrupt
            uint32_t mask_sec_u3_ac2_tx_trigger      : 1; // [18]   unmask sec-user 3 AC2 TX trigger interrupt
            uint32_t mask_sec_u3_ac3_tx_trigger      : 1; // [19]   unmask sec-user 3 AC3 TX trigger interrupt
            uint32_t mask_sec_u3_ac0_tx_buf_trigger  : 1; // [20]   unmask sec-user 3 AC0 TX buffer trigger interrupt
            uint32_t mask_sec_u3_ac1_tx_buf_trigger  : 1; // [21]   unmask sec-user 3 AC1 TX buffer trigger interrupt
            uint32_t mask_sec_u3_ac2_tx_buf_trigger  : 1; // [22]   unmask sec-user 3 AC2 TX buffer trigger interrupt
            uint32_t mask_sec_u3_ac3_tx_buf_trigger  : 1; // [23]   unmask sec-user 3 AC3 TX buffer trigger interrupt
            uint32_t reserved_24_30                  : 7; // [30:24]
            uint32_t master_sec_users_tx_int_en      : 1; // [31]   master secondary-users TX interrupt enable
        };
    } sec_users_tx_int_event_un_mask; // 0x36C

    uint32_t reserved_0x370_0x520[109]; // [0x370:0x520]

    uint32_t debug_bcn_s_ptr;    // 0x524  beacon TX status pointer (debug)
    uint32_t debug_ac0_s_ptr;    // 0x528  AC0 TX status pointer (debug)
    uint32_t debug_ac1_s_ptr;    // 0x52C  AC1 TX status pointer (debug)
    uint32_t debug_ac2_s_ptr;    // 0x530  AC2 TX status pointer (debug)
    uint32_t debug_ac3_s_ptr;    // 0x534  AC3 TX status pointer (debug)

    uint32_t reserved_0x538_0x53c[2]; // [0x538:0x53C]

    uint32_t debug_tx_c_ptr;     // 0x540  TX current pointer (debug)
    uint32_t debug_rx_pay_s_ptr; // 0x544  RX payload status pointer (debug)
    uint32_t debug_rx_hdr_c_ptr; // 0x548  RX header current pointer (debug)
    uint32_t debug_rx_pay_c_ptr; // 0x54C  RX payload current pointer (debug)

    uint32_t reserved_0x550_0x55c[4]; // [0x550:0x55C]

    // SW_PROFILING: each bit is a software-controlled profiling marker.
    // SW_SET_PROFILING: write 1 to a bit to set the corresponding bit in SW_PROFILING.
    // SW_CLEAR_PROFILING: write 1 to a bit to clear the corresponding bit in SW_PROFILING.
    uint32_t sw_profiling;       // 0x560  software profiling bits [31:0]
    uint32_t sw_set_profiling;   // 0x564  set software profiling bits; write 1 to set
    uint32_t sw_clear_profiling; // 0x568  clear software profiling bits; write 1 to clear

    uint32_t reserved_0x56c;     // 0x56C

    // Secondary-user TX current pointers; present only when the corresponding
    // MU-MIMO secondary-user path (RW_MUMIMO_SEC_USER{1,2,3}_EN) is compiled in.
    uint32_t debug_sec_u1_tx_c_ptr; // 0x570  secondary user 1 TX current pointer (debug)
    uint32_t debug_sec_u2_tx_c_ptr; // 0x574  secondary user 2 TX current pointer (debug)
    uint32_t debug_sec_u3_tx_c_ptr; // 0x578  secondary user 3 TX current pointer (debug)
} hw_mac_pl_t;

#define hw_mac_pl ((volatile hw_mac_pl_t *)MAC_PL_BASE_ADDR)
