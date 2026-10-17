#pragma once

// Runs the shell over Telnet. While a client is connected, stdio is tied to it and the UART
// takes no input; when it leaves, the UART is the console again.
void telnet_console_start();
