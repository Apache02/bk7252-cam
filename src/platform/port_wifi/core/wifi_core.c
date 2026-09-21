// WiFi core FreeRTOS tasks.
//
// Two tasks bridge the host application and the LMAC:
//
//   WiFi core task — drains the bmsg queue (same queue used by bmsg_*_sender
//                    functions in bmsg.c). Dispatches:
//                      BMSG_IOCTL_TYPE → ke_msg_send(arg)  (host→LMAC command)
//                      BMSG_RX_TYPE    → rxl_cntrl_evt(arg) (RX frame ready)
//                    After each message: ke_evt_core_scheduler() runs the LMAC
//                    software event loop.
//
//   kmsg task     — waits on app_sema, calls rwnx_recv_msg() +
//                   ke_evt_none_core_scheduler() to process LMAC→host replies.
//
// Call wifi_core_start() once after rwnxl_init() completes.

#include <stdint.h>
#include <stdbool.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>
#include "rtos.h"

#define DEBUG_NAME "wifi_core"
#include "debug.h"


#include "libip/rx.h"
#include "libip/tasks/mm.h"
#include "rwnx_intf.h"
#include "wifi/bmsg.h"
#include "wifi/core.h"
#include "wifi/net.h"

#undef count_of
#define count_of(x) (sizeof(x) / sizeof(x[0]))


// ---- app_sema (used by rwnx_intf.c::mr_kmsg_fwd) --------------------------
static SemaphoreHandle_t s_app_sema;

// imported: libip(ke_event.o)
void app_set_sema(void) {
    if (!s_app_sema) return;
    BaseType_t woken = pdFALSE;
    if (xPortIsInsideInterrupt()) {
        xSemaphoreGiveFromISR(s_app_sema, &woken);
        portYIELD_FROM_ISR(woken);
    } else {
        xSemaphoreGive(s_app_sema);
    }
}

// ---- WiFi core task --------------------------------------------------------

static StaticTask_t s_core_task_buf;
static StackType_t  s_core_stack[1024];
static TaskHandle_t s_core_task;

static void core_task_main(__unused void *arg) {
    bus_msg_t msg;

    while (1) {
        if (xQueueReceive(bmsg_get_queue(), &msg, portMAX_DELAY) != pdTRUE) continue;

        switch (msg.type) {
            case BMSG_IOCTL_TYPE: {
                // Peek the id before delivery — ke_msg_send() consumes the
                // message, so it cannot be read afterwards.
                uint16_t ke_id = ke_param2msg((void *)msg.arg)->id;
                ke_msg_send((void *)msg.arg);
                // MM_RESET_REQ makes the LMAC re-run rwnxl_init() internally,
                // which zeroes g_rwnx_connector. Re-register as soon as the
                // scheduler returns, or every later LMAC->host message is lost.
                ke_evt_core_scheduler();
                if (ke_id == MM_RESET_REQ) rwnx_connector_init();
                continue; // ke_evt_core_scheduler() already ran for this message
            }
            case BMSG_RX_TYPE:
                rxl_cntrl_evt((int)msg.arg);
                break;
            case BMSG_TX_TYPE:
                // One Ethernet frame from lwIP. Everything the archive is asked
                // to do has to happen here rather than on the tcpip thread. The
                // pbuf carries its own length, so len holds the interface index.
                wifi_net_tx_from_core((struct pbuf *)msg.arg, (uint8_t)msg.len);
                break;
            case BMSG_NULL_TYPE:
                break;
            default:
                LOG_W("unknown bmsg type=%lu", msg.type);
                break;
        }

        ke_evt_core_scheduler();
    }
}

// ---- kmsg task -------------------------------------------------------------

static StaticTask_t s_kmsg_task_buf;
static StackType_t  s_kmsg_stack[512];
static TaskHandle_t s_kmsg_task;

static StaticSemaphore_t s_sema_buf;

static void kmsg_task_main(__unused void *arg) {
    while (1) {
        xSemaphoreTake(s_app_sema, portMAX_DELAY);
        rwnx_recv_msg();
        ke_evt_none_core_scheduler();
    }
}

// ---- wifi_core_start -------------------------------------------------------
// Creates both tasks. Call once after rwnxl_init() completes.
void wifi_core_start(void) {
    if (s_app_sema) {
        LOG_W("wifi_core_start: already started");
        return;
    }

    s_app_sema = xSemaphoreCreateBinaryStatic(&s_sema_buf);

    // Order matters: registering the connector makes the LMAC start calling
    // mr_kmsg_fwd(), which pushes onto rw_msg_rx_head, so both list heads have to
    // be initialised first.
    mr_kmsg_init();
    rwnx_connector_init();

    // clang-format off
    s_core_task = xTaskCreateStatic(
        core_task_main,
        "wifi_core",
        count_of(s_core_stack),
        NULL,
        (configMAX_PRIORITIES - 3),
        s_core_stack,
        &s_core_task_buf
    );

    s_kmsg_task = xTaskCreateStatic(
        kmsg_task_main,
        "wifi_kmsg",
        count_of(s_kmsg_stack),
        NULL,
        (configMAX_PRIORITIES - 2),
        s_kmsg_stack,
        &s_kmsg_task_buf
    );
    // clang-format on

    LOG_I("started (core=%p kmsg=%p)", s_core_task, s_kmsg_task);
}

// The MAC block is not clocked and LMAC state is meaningless until the tasks
// exist, so diagnostics ask this before reading either.
bool wifi_core_running(void) { return s_app_sema != NULL; }
