#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// The firmware version, e.g. "v0.1.0" or "v0.1.0-3-g1a2b3c4" between tags.
const char *get_version();

void print_version();

#ifdef __cplusplus
}
#endif
