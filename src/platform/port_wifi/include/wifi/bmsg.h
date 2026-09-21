#pragma once

// The bus between the vendor binaries and the WiFi core task: bmsg.c holds the
// queue and the senders, wifi_core.c drains it. BUS_MSG_T layout comes from the
// SDK (func/bk_rtos/bk_rtos_pub.h) and is fixed by BMSG_ITEM_SIZE below.

#include <stdint.h>
#include <FreeRTOS.h>
#include <queue.h>

struct pbuf;

typedef enum {
    BMSG_NULL_TYPE   = 0,
    BMSG_RX_TYPE     = 1,
    BMSG_TX_TYPE     = 2,
    BMSG_IOCTL_TYPE  = 3,
    BMSG_SKT_TX_TYPE = 4,
    BMSG_MEDIA_TYPE  = 5,
} bmsg_type_t;

typedef struct {
    uint32_t type;
    uint32_t arg;
    uint32_t len;
    uint32_t sema; // beken_semaphore_t handle; unused on this port
} bus_msg_t;

QueueHandle_t bmsg_get_queue(void);

// 0 when the message was queued, non-zero when the queue was full and it was
// dropped — the one send failure a caller can learn about without waiting.
int bmsg_ioctl_sender(void *arg);

// Hands one outgoing frame to the WiFi core task, which is the only task allowed
// to call into the vendor binaries. The pbuf carries its own length, so the len
// slot of the message carries the interface index instead.
//
// Same return rule as above, and the reference the caller took on the pbuf is
// theirs again when this fails.
int bmsg_tx_sender(struct pbuf *p, uint8_t vif_idx);
