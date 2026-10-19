#include "version.h"

#include <stdio.h>

const char *get_version() { return BK_VERSION; }

void print_version() { printf("Bootloader %s by Apache02\r\n\r\n", get_version()); }
