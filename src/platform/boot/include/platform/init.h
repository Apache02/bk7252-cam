#ifndef _PLATFORM_INIT_H
#define _PLATFORM_INIT_H

// Register fn for __libc_init_array (runs before main). Lower priority
// runs first; INIT() without priority sorts after every INIT_AT(...).
// Priority is stringified verbatim — zero-pad to keep SORT() lexicographic
// order equal to numeric order (e.g. 01..99).
//
//   INIT_AT(intc_init, 01);    // first
//   INIT_AT(timer_init, 02);   // after intc
//   INIT(probe);               // after all *_AT entries
//
// PREINIT / PREINIT_AT register for __preinit_array — runs before
// __init_array (and before C++ global constructors). Use for things the
// init_array entries themselves depend on, like heap setup.
//
// FINI / FINI_AT are symmetric for __libc_fini_array (clean exit).

#define PREINIT(fn) __attribute__((section(".preinit_array"), used)) static typeof(fn) *__preinit_##fn = fn
#define PREINIT_AT(fn, prio) \
    __attribute__((section(".preinit_array." #prio), used)) static typeof(fn) *__preinit_##fn = fn
#define INIT(fn)          __attribute__((section(".init_array"), used)) static typeof(fn) *__init_##fn = fn
#define INIT_AT(fn, prio) __attribute__((section(".init_array." #prio), used)) static typeof(fn) *__init_##fn = fn
#define FINI(fn)          __attribute__((section(".fini_array"), used)) static typeof(fn) *__fini_##fn = fn
#define FINI_AT(fn, prio) __attribute__((section(".fini_array." #prio), used)) static typeof(fn) *__fini_##fn = fn

#endif // _PLATFORM_INIT_H
