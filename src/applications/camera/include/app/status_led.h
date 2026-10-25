#pragma once

// Starts the task that shows the Wi-Fi state on the LED: one short flash every 2 s with an address
// on the station, two flashes without one, a slow blink with only the access point, dark otherwise.
void status_led_start();

// While true the LED stays on, whatever the Wi-Fi state is. For a short message from another task.
void status_led_hold(bool on);
