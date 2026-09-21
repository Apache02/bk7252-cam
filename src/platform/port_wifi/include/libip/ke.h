#pragma once

// ip/ke/ — the RivieraWaves kernel inside libip_7221u.a: its tasks, its
// messages, its event word and its one global environment. Everything the
// archive's LMAC and UMAC halves run on.
//
// Layouts, ids and signatures are dictated by the archive and were read out of
// its own DWARF (ke_task.o, ke_msg.o, ke_event.o). See docs/wifi_rw.md for what
// each piece does.

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "libip/co_list.h"


#ifdef __cplusplus
extern "C" {
#endif

// ---- Kernel identifier types (ke_msg.h) ------------------------------------

typedef uint16_t ke_task_id_t;
typedef uint16_t ke_msg_id_t;
typedef uint16_t ke_state_t;
typedef uint32_t evt_field_t;

// A task id carries an instance index in its high byte, so a task with several
// instances is addressed as one id. Every task this port talks to has a single
// instance, which is why plain TASK_* values work as destinations.
#define KE_BUILD_ID(type, index) ((ke_task_id_t)(((index) << 8) | (type)))
#define KE_TYPE_GET(task_id)     ((task_id) & 0xFF)
#define KE_IDX_GET(task_id)      (((task_id) >> 8) & 0xFF)

// ---- Tasks (enum ke_task_id, ke_task.o) ------------------------------------
// The boundary at TASK_LAST_EMB is load-bearing: ke_msg_send() compares a
// message's dest_id against it and nothing else. At or below, the message goes
// onto the kernel's own queue_sent; above, straight out to the host connector.
//
// TDLS and MESH have no slot here — neither was compiled into this archive.

enum ke_task_id {
    TASK_NONE = 255,

    TASK_MM    = 0, // hardware, channels, virtual interfaces
    TASK_SCAN  = 1, // the LMAC's own channel-by-channel sweep
    TASK_SCANU = 2, // scan sessions and join
    TASK_ME    = 3, // per-station config, management TX, rate control
    TASK_SM    = 4, // station state machine
    TASK_APM   = 5, // AP mode — compiled in, unused by this port
    TASK_BAM   = 6, // Block Ack agreements
    TASK_RXU   = 7, // fans incoming management frames out to the tasks above

    TASK_LAST_EMB = TASK_RXU,

    TASK_API = 8, // us
    TASK_MAX = 9,
};

// Where a task's block of message ids starts. Each task header in libip/tasks/
// opens its enum with this, so an id is its position in that task's list and
// never a hand-written number.
#define KE_FIRST_MSG(task) ((ke_msg_id_t)((task) << 10))

// ---- Messages (ke_msg.h, ke_msg.o) -----------------------------------------

// 16 bytes: hdr 0, id 4, dest_id 6, src_id 8, param_len 10, param 12.
struct ke_msg {
    struct co_list_hdr hdr;
    ke_msg_id_t        id;
    ke_task_id_t       dest_id;
    ke_task_id_t       src_id;
    uint16_t           param_len;
    uint32_t           param[1];
};
static_assert(sizeof(struct ke_msg) == 16, "ke_msg.o DWARF: ke_msg is 16 bytes");

// What a task's handler returns, and what decides who frees the message.
enum ke_msg_status_tag {
    KE_MSG_CONSUMED = 0, // the kernel frees it
    KE_MSG_NO_FREE,      // consumed, nothing freed
    KE_MSG_SAVED,        // wrong state to act on it — retried on the next state change
};

// A request is passed around by its parameter block, so these two convert
// between that and the message carrying it.
static inline struct ke_msg *ke_param2msg(void const *param_ptr) {
    if (param_ptr == 0) return 0;
    return (struct ke_msg *)((uint8_t *)param_ptr - offsetof(struct ke_msg, param));
}
static inline void *ke_msg2param(struct ke_msg const *msg) {
    return (void *)((uint8_t *)msg + offsetof(struct ke_msg, param));
}

// Returns the parameter block, already zeroed over param_len bytes — ke_msg.o
// ends with memset(param, 0, param_len).
void *ke_msg_alloc(ke_msg_id_t id, ke_task_id_t dest_id, ke_task_id_t src_id, uint16_t param_len);
void  ke_msg_free(struct ke_msg *msg);
void  ke_msg_send(void const *param_ptr);
void  ke_msg_send_basic(ke_msg_id_t id, ke_task_id_t dest_id, ke_task_id_t src_id);

static inline void ke_msg_free_by_param(void const *param_ptr) { ke_msg_free(ke_param2msg(param_ptr)); }

// ---- Task state (ke_task.h, ke_task.o) -------------------------------------

ke_state_t ke_state_get(ke_task_id_t id);
void       ke_state_set(ke_task_id_t id, ke_state_t state);

// ---- Events (ke_event.h, ke_event.o) ---------------------------------------
// evt_field is a software interrupt-pending word: one bit per event, highest
// set bit served first. Event N occupies bit 31 - N, so KE_EVT_RESET is the top
// bit, not bit 0 — never pass a bare enum value to the ke_evt_* calls.

enum ke_evt_type {
    KE_EVT_RESET = 0, // the LMAC's reconfiguration handler, not a fault reset
    KE_EVT_MM_TIMER,
    KE_EVT_KE_TIMER,
    KE_EVT_TXL_PAYLOAD_BCN,
    KE_EVT_TXL_PAYLOAD_AC3,
    KE_EVT_TXL_PAYLOAD_AC2,
    KE_EVT_TXL_PAYLOAD_AC1,
    KE_EVT_TXL_PAYLOAD_AC0,
    KE_EVT_KE_MESSAGE,
    KE_EVT_HW_IDLE,
    KE_EVT_PRIMARY_TBTT,
    KE_EVT_SECONDARY_TBTT,
    KE_EVT_RXUREADY,
    KE_EVT_TXFRAME_CFM,
    KE_EVT_TXCFM_BCN,
    KE_EVT_TXCFM_AC3,
    KE_EVT_TXCFM_AC2,
    KE_EVT_TXCFM_AC1,
    KE_EVT_TXCFM_AC0,
    KE_EVT_GP_DMA_DL,
    KE_EVT_EVM_VIA_MAC_TEST,
    KE_EVT_MAX,
};

#define KE_EVT_BIT(evt)  ((evt_field_t)(1u << (31 - (evt))))
#define KE_EVT_RESET_BIT KE_EVT_BIT(KE_EVT_RESET)

// The kernel's whole mutable state, 28 bytes, and nothing else is global to it.
// queue_sent holds delivered-but-not-dispatched messages, queue_saved the ones
// a task was in the wrong state for, queue_timer the software timers.
struct ke_env_tag {
    volatile evt_field_t evt_field;
    struct co_list       queue_sent;
    struct co_list       queue_saved;
    struct co_list       queue_timer;
};
static_assert(sizeof(struct ke_env_tag) == 28, "ke_event.o DWARF: ke_env_tag is 28 bytes");

extern struct ke_env_tag ke_env;

// Raising a bit is also what wakes the host: on an empty evt_field ke_evt_set()
// calls bmsg_null_sender(), and on a bit outside core_evt_mask it calls
// app_set_sema() instead. A bit raised while the field is already non-empty
// wakes nobody, on the assumption a scheduler run is already coming.
void ke_evt_set(evt_field_t events);
void ke_evt_clear(evt_field_t events);

static inline evt_field_t ke_evt_get(void) { return ke_env.evt_field; }

// Which payload-ready bit belongs to an access category.
uint32_t ke_get_ac_payload_bit(uint32_t access_category);

// The three ways to run the event loop. core_evt_mask splits the field between
// the two schedulers; it is 0xFFFFFFFF in this build, so the core one runs
// everything and the none-core one is left with nothing.
void ke_evt_schedule(void);
void ke_evt_core_scheduler(void);
void ke_evt_none_core_scheduler(void);
void ke_evt_mask_schedule(uint32_t mask);

extern uint32_t core_evt_mask;

#ifdef __cplusplus
}
#endif
