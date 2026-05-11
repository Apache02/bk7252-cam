#ifndef _COMMON_DEFS_H
#define _COMMON_DEFS_H

#include <stdint.h>
#include <sys/cdefs.h>


// hw_write_fields - atomic multi-field write to a memory-mapped register.
//
// Builds a register value from named bitfield initialisers and commits it
// with a single 32-bit store via the union's `.v` alias. Replaces the
// "sequence of bitfield assignments" pattern, where each assignment compiles
// to its own read-modify-write and the hardware briefly sees intermediate
// values.
//
// Requires `reg` to be a union with a `.v` 32-bit alias, which is the
// project-wide convention for register definitions (see any *_regs.h).
// Unlisted fields default to zero (per C designated-initialiser rules).
//
// Example:
//   hw_write_fields(hw_efuse->ctrl,
//       .en = 1,
//       .addr = a,
//   );
//
// Naming follows the pico-sdk hw_* convention for register helpers.
#define hw_write_fields(reg, ...) do {                  \
    typeof(reg) _tmp = { __VA_ARGS__ };                 \
    (reg).v = _tmp.v;                                   \
} while (0)


#endif // _COMMON_DEFS_H
