#pragma once

#include <stdio.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


int hex_nibble(char c);

void hex_decode(const char *hex, uint8_t *out, size_t out_len);

void print_hex(const uint8_t *data, size_t len);


#ifdef __cplusplus
}
#endif
