#pragma once

// Starts the task that reads the two keys.
//
// Power key: two short presses in a row reboot the board.
// Mode key: a short press toggles the access point until the next reboot; holding it for 10 seconds
// resets the settings. The LED stays on from the 2nd second of the hold as a warning.
void keys_start();
