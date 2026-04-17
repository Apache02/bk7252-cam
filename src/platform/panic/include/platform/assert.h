#ifndef _PLATFORM_ASSERT_H
#define _PLATFORM_ASSERT_H

#include "platform/panic.h"

#define assert_true(condition, message)     if (!(condition)) { panic(message); }
#define assert_false(condition, message)    if (condition) { panic(message); }

#endif // _PLATFORM_ASSERT_H
