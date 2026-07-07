#ifndef _PLATFORM_SCHED_H
#define _PLATFORM_SCHED_H

// newlib's <sched.h> guards sched_yield()'s declaration behind _POSIX_THREADS /
// _POSIX_PRIORITY_SCHEDULING; neither is defined project-wide, so define the narrower
// one here, scoped to this translation unit, just to unlock the declaration.
#define _POSIX_PRIORITY_SCHEDULING
#include <sched.h>

#endif // _PLATFORM_SCHED_H
