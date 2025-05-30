#ifndef _PLATFORM_UTILS_PANIC_H
#define _PLATFORM_UTILS_PANIC_H

#ifdef __cplusplus
extern "C" {
#endif

void panic(const char *message);

void panic_blink(int count);

#ifdef __cplusplus
}
#endif

#endif // _PLATFORM_UTILS_PANIC_H