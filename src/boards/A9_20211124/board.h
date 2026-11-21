#pragma once

// Board print: 20211124, camera flex 09 v1.0 / LT1818
// UART1, Camera: GC0329 on I2C2, IR flashlight, 2 buttons, 1 LED, 1 charging LED

#define BOARD_SOC_CHIP bk7221u

// The UART the console (stdio, shell, bootloader) runs on: 1 or 2.
// UART2 shares P0/P1 with I2C2, which the camera uses.
#define BOARD_STDIO_UART (1)

#define LED_PIN                 (13) // blue
#define LED_PIN_INVERTED        (0)
#define FLASHLIGHT_PIN          (24)
#define FLASHLIGHT_PIN_INVERTED (0)
#define KEY_PWR_PIN             (7)
#define KEY_MODE_PIN            (4)

#define CAMERA_POWER_METHOD      "VDDRAM"
#define CAMERA_POWER_VDDRAM_VOLT SCTRL_VDDRAM_3V5
#define CAMERA_I2C               "i2c2"

/*
 * Pin setup of the vendor firmware (BK7252_A9_Camera_20211124_SDK_3.0.33), seen in the emulator.
 * Roles in brackets are guesses. The LED and keys were traced on the board (issue #5).
 *
 *  pin     | vendor setup        | role
 * ---------+---------------------+-------------------------
 *  0, 1    | peripheral, pull-up | I2C2 SCL/SDA (camera)
 *  4       | input, pull-up      | mode key
 *  7       | input, pull-up      | power key
 *  9       | output low          | unknown
 *  10, 11  | peripheral, pull-up | UART1 (console)
 *  12      | input               | unknown
 *  13      | output low          | blue LED
 *  14,16,17| peripheral, pull-up | (SD card; its init fails)
 *  24      | output low          | IR flashlight, off
 *  27,29-39| peripheral          | DVP camera bus
 *
 * The red LED belongs to the battery charger (package pin 12, VUSB) and is not a GPIO.
 *
 * The vendor probes 0x21 on I2C2 first; with no answer there it finds the next sensor, so a
 * GC0329 at 0x31 was not reached in the emulator run (an HI704 stood in for the sensor).
 */
