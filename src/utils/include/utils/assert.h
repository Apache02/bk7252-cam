#ifndef _PLATFORM_UTILS_ASSERT_H
#define _PLATFORM_UTILS_ASSERT_H

#include "panic.h"

#define assert_true(condition, message)     if (!(condition)) { panic(message); }
#define assert_false(condition, message)    if (condition) { panic(message); }

#endif // _PLATFORM_UTILS_ASSERT_H