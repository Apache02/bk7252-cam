#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <FreeRTOS.h>
#include <queue.h>

#include "wifi/bmsg.h"

#define DEBUG_NAME "bmsg"
#include "debug.h"

// Message bus for the WiFi core task. The vendor binaries push through the
// bmsg_*_sender entry points; wifi_core.c drains the queue.

#define BMSG_QUEUE_LEN 64
#define BMSG_ITEM_SIZE sizeof(bus_msg_t)

static StaticQueue_t s_queue_static;
static uint8_t       s_queue_buf[BMSG_QUEUE_LEN * BMSG_ITEM_SIZE];
static QueueHandle_t s_queue;

// Created by whichever caller arrives first, and that is not necessarily a task:
// rwnxl_init() runs before wifi_core_start(), so the LMAC can already be calling
// the senders below from the MAC FIQ.
QueueHandle_t bmsg_get_queue(void) {
    if (!s_queue) s_queue = xQueueCreateStatic(BMSG_QUEUE_LEN, BMSG_ITEM_SIZE, s_queue_buf, &s_queue_static);
    return s_queue;
}

// Returns false when the queue was full and the message was dropped.
static bool push(bus_msg_t *msg) {
    QueueHandle_t q = bmsg_get_queue();
    // Must start false: FreeRTOS only ever *sets* this flag to pdTRUE, so an
    // uninitialised value yields from the ISR on every push that woke nobody.
    BaseType_t woken = pdFALSE;
    BaseType_t ok;

    if (xPortIsInsideInterrupt()) {
        ok = xQueueSendToBackFromISR(q, msg, &woken);
        portYIELD_FROM_ISR(woken);
    } else {
        ok = xQueueSendToBack(q, msg, 0);
    }

    if (ok != pdTRUE) LOG_W("%s: queue full, type=%lu dropped", __func__, (unsigned long)msg->type);
    return ok == pdTRUE;
}

// imported: libip(ke_event.o) — wake the WiFi core task with a null message
void bmsg_null_sender(void) {
    if (uxQueueMessagesWaiting(bmsg_get_queue()) > 0) return;
    bus_msg_t msg = {.type = BMSG_NULL_TYPE};
    push(&msg);
}

// imported: libip(rxl_cntrl.o) — an RX frame is ready
void bmsg_rx_sender(void *arg) {
    bus_msg_t msg = {.type = BMSG_RX_TYPE, .arg = (uint32_t)arg};
    push(&msg);
}

// Called by ethernetif.c from the tcpip thread.
int bmsg_tx_sender(struct pbuf *p, uint8_t vif_idx) {
    bus_msg_t msg = {
        .type = BMSG_TX_TYPE,
        .arg  = (uint32_t)p,
        .len  = vif_idx,
    };
    return push(&msg) ? 0 : -1;
}

// Called by rw_msg.c after building a ke_msg — hands the pointer to the WiFi
// core task, which delivers it with ke_msg_send(). Non-zero means the message
// never reached the queue, so nothing will ever be sent for it.
int bmsg_ioctl_sender(void *arg) {
    bus_msg_t msg = {.type = BMSG_IOCTL_TYPE, .arg = (uint32_t)arg};
    return push(&msg) ? 0 : -1;
}
