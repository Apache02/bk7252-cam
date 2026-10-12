#include <stdio.h>

#ifndef BOOTLOADER_VERSION
#define BOOTLOADER_VERSION "unknown"
#endif

void print_version() {
    printf("Bootloader by Apache02 "
           "(built on " BOOTLOADER_VERSION " at " __DATE__ " " __TIME__ ")"
           "\r\n\r\n");
}
