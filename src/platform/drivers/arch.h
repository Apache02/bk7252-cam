#ifndef _ARCH_H_
#define _ARCH_H_

#include "typedef.h"

#define REG_READ(addr)              (*((volatile UINT32 *)(addr)))
#define REG_WRITE(addr, _data)      (*((volatile UINT32 *)(addr)) = (_data))

#define BIT(i)                      (1UL << (i))


#endif // _ARCH_H_