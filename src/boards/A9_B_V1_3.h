#pragma once

/**
 * Board print: A9_B
 *              V1.3_2205i6
 *
 * UART1 + UART2 available
 * Camera Sensor: HI704 on I2C1
 * 2 buttons
 * one led at gpio 26
 * one charging led
 */


#define LED_PIN         (26)
#define KEY_PWR_PIN     (2)       // pull up
#define KEY_MODE_PIN    (7)       // pull up

