#ifndef _PLATFORM_UNISTD_H
#define _PLATFORM_UNISTD_H

#include <sys/types.h>

// newlib's <unistd.h> does not declare usleep() under the project's language standards.
#ifdef __cplusplus
extern "C" {
#endif

// Sleeps for at least `us` microseconds, rounded up to whole milliseconds. Always returns 0.
int usleep(useconds_t us);

#ifdef __cplusplus
}
#endif

#endif // _PLATFORM_UNISTD_H
