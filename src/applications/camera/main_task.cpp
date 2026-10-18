#include "app/main_task.h"

#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>

#include "hardware/sctrl.h"

#include "app/camera_web.h"
#include "app/config.h"
#include "Service.h"
#include "net.h"
#include "app/telnet_console.h"
#include "platform/telnet_server.h"
#include "wifi/core.h"
#include "wifi/net.h"
#include "wifi/ap.h"
#include "wifi/sta.h"

// #define BACKOFF_MIN_MS  2000
// #define BACKOFF_MAX_MS  30000
// #define MAX_FAILURES    5
// #define ADDRESS_WAIT_MS 15000
// #define AP_RETRY_MS     5000
// #define POLL_MS         200
// #define MONITOR_MS      500

// The task starts above everything so the bring-up is not interrupted, then steps down: joining a
// network hashes the passphrase for half a second, which must not starve the shell and the stream.
#define PRIORITY_INIT (configMAX_PRIORITIES - 1)
#define PRIORITY_RUN  (configMAX_PRIORITIES - 3)

#define TELNET_PORT 23

// enum class Request : uint8_t { None, Ap, Sta };
//
#define count_of(x) (sizeof(x) / sizeof(x[0]))

static StaticTask_t taskTCB;
static StackType_t  taskStack[configMINIMAL_STACK_SIZE * 6];
static TaskHandle_t s_task = nullptr;

static volatile bool s_ready          = false;
static volatile bool s_config_changed = false;

static void print_mac(const char *name, const uint8_t mac[6]) {
    printf("%s MAC: %02x:%02x:%02x:%02x:%02x:%02x\r\n", name, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void bring_up() {
    // Starts the tcpip thread; it has to run before any interface is created.
    net_init();

    config_load();

    sctrl_dpll_int_open();
    sctrl_rf_init();
    rwnxl_init();
    wifi_core_start();

    vTaskDelay(100);
    camera_web_start();
}

// Brings the access point to the requested state. Returns true when it is there.
static Service service_ap(const bool target, const ApConfig config, const int max_attempts = 10) {
    if (wifi_ap_is_up() == target) co_return true;

    for (int attempt = 1; attempt <= max_attempts; attempt++) {
        if (target) {
            // Wi-Fi Access point -> UP
            wifi_ap_start(&config);

            for (int i = 0; i < 10; i++) {
                co_await sleep_ms(1000);
                if (wifi_ap_is_up()) {
                    printf("[wifi] AP \"%s\" started on channel %d\r\n", config.ssid, config.channel);
                    co_return true;
                }
            }

            wifi_ap_stop();
            co_await sleep_ms(1000);
        } else {
            // Wi-Fi Access point -> DOWN
            wifi_ap_stop();
            co_await sleep_ms(1000);
            if (!wifi_ap_is_up()) {
                printf("[wifi] AP stopped\r\n");
                co_return true;
            }
        }
    }

    printf("[wifi] error: can't %s AP\r\n", target ? "start" : "stop");
    co_return false;
}

static Service service_sta(const bool target, const StaConfig config, const int max_attempts = 10) {
    if (target ? (wifi_sta_is_up() && wifi_sta_is_link_up()) : !wifi_sta_is_up()) co_return true;

    printf("[wifi] connecting to: %s %s\r\n", config.ssid, config.passphrase ? "(with password)" : "(open)");

    for (int attempt = 1; attempt <= max_attempts; attempt++) {
        if (target) {
            // Wi-Fi Station -> Connected To config.ssid
            if (wifi_sta_is_up()) {
                // did not have IP
                wifi_sta_stop();
                co_await sleep_ms(1000);
                continue;
            }

            Shared<Promise<ConnectResult>> promise = wifi_sta_start(&config);
            if (promise.empty()) {
                printf("[wifi] association not started\r\n");
                co_return false;
            }

            co_await sleep_ms(2000);

            Owned<ConnectResult> result = promise->await();
            if (result.empty()) {
                printf("[wifi] association produced no result\r\n");
                co_return false;
            }

            switch (result->status) {
                case Associated:
                    printf("[wifi] associated with %02x:%02x:%02x:%02x:%02x:%02x\r\n", result->bssid[0],
                           result->bssid[1], result->bssid[2], result->bssid[3], result->bssid[4], result->bssid[5]);
                    printf("[wifi] asking for an address\r\n");

                    for (int i = 0; i < 20; i++) {
                        co_await sleep_ms(1000);
                        if (wifi_sta_get_address(nullptr, nullptr, nullptr)) {
                            printf("[wifi] STA got an address\r\n");
                            co_return true;
                        }
                    }

                    break;
                case Refused:
                    printf("[wifi] the AP refused, 802.11 status %u\r\n", result->status_code);
                    break;
                case NoReply:
                    printf("[wifi] no answer before the deadline\r\n");
                    break;
                case AuthFailed:
                    printf("[wifi] the handshake failed: wrong passphrase, or the AP stopped answering\r\n");
                    break;
            }

        } else {
            // Wi-Fi Station -> Off
            wifi_sta_stop();
            co_await sleep_ms(1000);
            if (!wifi_sta_is_up()) {
                co_return true;
            }
        }

        wifi_sta_stop();
        co_await sleep_ms(1000);
    }

    printf("[wifi] error: can't %s STA\r\n", target ? "start" : "stop");
    co_return false;
}

static Service service_telnet(bool target, const int max_attempts = 10) {
    if (target == telnet_server_is_up()) co_return true;

    for (int attempt = 1; attempt <= max_attempts; attempt++) {
        if (target) {
            telnet_console_start(TELNET_PORT);
        } else {
            telnet_server_stop();
        }
        co_await sleep_ms(1000);
        if (target == telnet_server_is_up()) {
            if (target) {
                printf("[telnet] listening on *:%u\r\n", TELNET_PORT);
            } else {
                printf("[telnet] server stopped\r\n");
            }
            co_return true;
        }
    }

    printf("[telnet] error: can't %s\r\n", target ? "start" : "stop");
    co_return false;
}

static void configure_mac() {
    int delay = 0;
    if (wifi_ap_is_up()) {
        wifi_ap_stop();
        delay = 200;
    }
    if (wifi_sta_is_up()) {
        wifi_sta_stop();
        delay = 200;
    }
    vTaskDelay(delay);

    // The address goes to the radio when the interface comes up, so it has to be set before that.
    uint8_t ap_mac[6];
    config_ap_mac(ap_mac);
    wifi_net_set_mac(WIFI_IFACE_STA, g_config.mac);
    wifi_net_set_mac(WIFI_IFACE_AP, ap_mac);
    print_mac("STA", g_config.mac);
    print_mac("AP", ap_mac);
}

static __dead2 void main_task(__unused void *params) {
    bring_up();
    configure_mac();

    vTaskPrioritySet(nullptr, PRIORITY_RUN);
    s_ready = true;

    Service ap     = {};
    Service sta    = {};
    Service telnet = {};

    auto     configure_required = true;
    Service *services[]         = {&ap, &sta, &telnet};

    for (;;) {
        if (configure_required) {
            configure_required = false;
            ap  = service_ap(g_config.ap_enabled, {.ssid = g_config.ap_ssid, .channel = g_config.ap_channel}, 10);
            sta = service_sta(g_config.sta_enabled,
                              {
                                  .ssid       = g_config.sta_ssid,
                                  .passphrase = g_config.sta_password[0] ? g_config.sta_password : nullptr,
                                  .bssid      = {0},
                                  .channel    = 0,
                              },
                              10);

            telnet = service_telnet(g_config.telnet_enabled);
        }

        bool busy = false;
        for (const auto service : services) {
            if (!service->empty()) {
                busy = true;
                service->poll();
                if (service->done()) service->reset();
            }
        }

        // main_task_config_changed() wakes it earlier. With no running services it sleeps until then.
        ulTaskNotifyTake(pdTRUE, busy ? pdMS_TO_TICKS(500) : pdMS_TO_TICKS(60000));

        if (s_config_changed) {
            s_config_changed   = false;
            configure_required = true;
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (!busy && g_config.sta_enabled && g_config.sta_ssid[0] != '\0' && !wifi_sta_is_link_up()) {
            printf("[wifi] STA link lost, reconnecting\r\n");
            sta = service_sta(g_config.sta_enabled,
                              {
                                  .ssid       = g_config.sta_ssid,
                                  .passphrase = g_config.sta_password[0] ? g_config.sta_password : nullptr,
                                  .bssid      = {0},
                                  .channel    = 0,
                              },
                              1);
        }
    }
}

void main_task_start() {
    // clang-format off
    s_task = xTaskCreateStatic(
        main_task,
        "main_service",
        count_of(taskStack),
        nullptr,
        PRIORITY_INIT,
        taskStack,
        &taskTCB
    );
    // clang-format on
}

bool main_task_ready() { return s_ready; }

void main_task_config_changed() {
    s_config_changed = true;
    if (s_task) xTaskNotifyGive(s_task);
}
