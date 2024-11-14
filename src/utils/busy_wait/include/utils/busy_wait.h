#ifndef PLATFORM_UTILS_BUSY_WAIT_H
#define PLATFORM_UTILS_BUSY_WAIT_H

#ifdef __cplusplus
extern "C" {
#endif

void busy_wait_us(unsigned int us);

void busy_wait_ms(unsigned int ms);

void busy_wait(unsigned int sec);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_UTILS_BUSY_WAIT_H