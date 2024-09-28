#ifndef _PLATFORM_UTILS_SLEEP_H_
#define _PLATFORM_UTILS_SLEEP_H_

#ifdef __cplusplus
extern "C" {
#endif

void usleep(unsigned int us);

void msleep(unsigned int ms);

void sleep(unsigned int sec);

#ifdef __cplusplus
}
#endif

#endif // _PLATFORM_UTILS_SLEEP_H_