#pragma once

// Board print: A9_B / V1.3_2205i6
// UART1 + UART2, Camera: HI704 on I2C1, 2 buttons, 1 LED, 1 charging LED

#define BOARD_SOC_CHIP bk7221u

#define LED_PIN      (26)
#define KEY_PWR_PIN  (2)
#define KEY_MODE_PIN (7)

#define CAMERA_I2C       "i2c1"
#define CAMERA_RESET_PIN 28 // not sure about role of this pin

/*
 * camera connection
 *
 * The sensor power pin is the chip's VDDRAM output (sctrl_vddram_enable()), not a GPIO.
 *
 * chip side                   ||         sensor side
 *        role    | name | pin ||
 * ---------------+------+-----||----+----------------
 *         VDDRAM |      |     || 01 | power supply
 *       I2C1 SDA |  P21 | 40  ||  . |
 *       I2C1 SCL |  P20 | 41  ||  . |
 *         GPIO28 |  P28 |     ||  . |
 *           PXD7 |  P39 | 29  ||  . |
 *           PXD6 |  P38 | 28  ||  . |
 *           PXD5 |  P37 | 27  ||  . |
 *           PXD4 |  P36 | 26  ||  . |
 *           PXD3 |  P35 | 25  ||  . |
 *           PXD2 |  P34 | 24  ||  . |
 *          VSYNC |  P31 | 23  ||  . |
 *          HSYNC |  P30 | 22  ||  . |
 *       CIS_MCLK |  P27 | 21  ||  . |
 *                |      |     ||  . | GND
 *           PCLK |  P29 | 20  ||  . |
 *                |      |     ||  . | GND
 *           PXD0 |  P32 | 19  ||  . |
 *           PXD1 |  P33 | 18  || 18 |
 * ---------------+------+-----||----+----------------
 */
