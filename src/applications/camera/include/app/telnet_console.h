#pragma once

#include <stdint.h>

// Runs the shell over Telnet. While a client is connected, stdio is tied to it and the UART
// takes no input; when it leaves, the UART is the console again.
bool telnet_console_start(const uint16_t port);
