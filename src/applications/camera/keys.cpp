#include "app/keys.h"

#include <stdint.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>

#include "board.h"
#include "hardware/gpio.h"
#include "hardware/wdt.h"

#include "app/config.h"
#include "app/main_task.h"
#include "app/status_led.h"

#define count_of(x) (sizeof(x) / sizeof(x[0]))

// Each key is optional: a board without its pin has no such key, and with neither the task is
// not started.
#if defined(KEY_PWR_PIN) || defined(KEY_MODE_PIN)
#define KEYS_ENABLED
#endif

#ifdef KEYS_ENABLED
constexpr uint32_t TICK_MS       = 50;
constexpr uint8_t  DEBOUNCE_TICKS = 2;
// A press shorter than this is a tap, and two taps closer than DOUBLE_TAP_MS reboot the board.
constexpr uint32_t TAP_MS        = 500;
constexpr uint32_t DOUBLE_TAP_MS = 800;
// A press shorter than this toggles the access point; past it the LED warns of a reset.
constexpr uint32_t WARNING_MS = 2000;
constexpr uint32_t RESET_MS   = 10000;

struct Button {
    gpio_num_t pin;
    bool       down      = false; // debounced
    uint8_t    count     = 0;     // samples in a row that disagree with `down`
    uint32_t   held      = 0;     // ticks since the press
    uint32_t   last_held = 0;     // `held` at the last release
};

// Samples the pin once. Returns true when the debounced state has just changed.
static bool button_update(Button &button) {
    const bool pressed = gpio_get(button.pin) == 0;

    if (pressed == button.down) {
        button.count = 0;
    } else if (++button.count >= DEBOUNCE_TICKS) {
        button.down      = pressed;
        button.count     = 0;
        button.last_held = button.held;
        button.held      = 0;
        return true;
    }

    if (button.down) button.held++;
    return false;
}

#ifdef KEY_MODE_PIN
static void blink_led(const int times) {
    for (int i = 0; i < times; i++) {
        status_led_hold(true);
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
        status_led_hold(false);
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}

// Runtime only: the stored config is not touched, so a reboot brings back the saved state.
static void toggle_ap() {
    taskENTER_CRITICAL();
    g_config.ap_enabled = !g_config.ap_enabled;
    const bool enabled  = g_config.ap_enabled;
    taskEXIT_CRITICAL();

    printf("[keys] mode: AP %s\r\n", enabled ? "on" : "off");
    main_task_config_changed();
}

static void reset_settings() {
    printf("[keys] mode held: resetting settings\r\n");

    const int error = config_reset();
    if (error) {
        printf("[keys] reset failed: %d\r\n", error);
    } else {
        blink_led(5);
    }
    main_task_config_changed();
}
#endif // KEY_MODE_PIN

#ifdef KEY_PWR_PIN
static __dead2 void reboot_board() {
    printf("[keys] power double tap: reboot\r\n");
    vTaskDelay(pdMS_TO_TICKS(100));

    taskENTER_CRITICAL();
    wdt_reboot(1000);
    taskEXIT_CRITICAL();
}
#endif // KEY_PWR_PIN

static __dead2 void keys_task(__unused void *params) {
#ifdef KEY_PWR_PIN
    gpio_config(KEY_PWR_PIN, GPIO_IN_PULLUP);
    Button power = {.pin = KEY_PWR_PIN};

    bool     tapped   = false;
    uint32_t last_tap = 0;
#endif

#ifdef KEY_MODE_PIN
    gpio_config(KEY_MODE_PIN, GPIO_IN_PULLUP);
    Button mode = {.pin = KEY_MODE_PIN};

    // Set once the hold has reset the settings, so one press resets only once.
    bool reset_done = false;
#endif

    __unused uint32_t tick = 0;
    TickType_t        wake = xTaskGetTickCount();
    for (;; tick++) {
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(TICK_MS));

#ifdef KEY_PWR_PIN
        // The board restarts on the release of the second tap, so holding the key through a boot
        // does not loop.
        if (button_update(power) && !power.down) {
            const bool tap = power.last_held * TICK_MS < TAP_MS;
            if (tap && tapped && (tick - last_tap) * TICK_MS <= DOUBLE_TAP_MS) reboot_board();
            tapped   = tap;
            last_tap = tick;
        }
#endif // KEY_PWR_PIN

#ifdef KEY_MODE_PIN
        if (button_update(mode) && !mode.down) {
            if (!reset_done && mode.last_held * TICK_MS < WARNING_MS) toggle_ap();
            reset_done = false;
        }

        if (mode.down && !reset_done) {
            const uint32_t held_ms = mode.held * TICK_MS;
            if (held_ms >= RESET_MS) {
                reset_done = true;
                status_led_hold(false);
                reset_settings();
            } else {
                status_led_hold(held_ms >= WARNING_MS);
            }
        } else if (!mode.down) {
            status_led_hold(false);
        }
#endif // KEY_MODE_PIN
    }
}

static StaticTask_t taskTCB;
static StackType_t  taskStack[configMINIMAL_STACK_SIZE * 6];
#endif // KEYS_ENABLED

void keys_start() {
#ifdef KEYS_ENABLED
    // clang-format off
    xTaskCreateStatic(
        keys_task,
        "keys",
        count_of(taskStack),
        nullptr,
        configMAX_PRIORITIES - 4,
        taskStack,
        &taskTCB
    );
    // clang-format on
#endif
}
