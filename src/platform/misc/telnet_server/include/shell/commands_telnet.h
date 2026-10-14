#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Disconnects the Telnet client the shell is running for.
int command_telnet_exit(int argc, const char *argv[]);

#ifdef __cplusplus
}
#endif
