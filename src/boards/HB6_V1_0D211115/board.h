#pragma once

// Board print: HB6 / V1.0D211115
// UART1, Camera on I2C2; camera wiring not known yet

#define BOARD_SOC_CHIP bk7221u

// The UART the console (stdio, shell, bootloader) runs on: 1 or 2.
#define BOARD_STDIO_UART (1)

#define LED_PIN                 (4)
#define LED_PIN_INVERTED        (1)
#define KEY_PWR_PIN             (5)
#define KEY_MODE_PIN            (13)

#define CAMERA_I2C "i2c2"
