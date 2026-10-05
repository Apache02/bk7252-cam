#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "platform/sched.h"
#include "platform/unistd.h"
#include "platform/cpu.h"
#include "platform/timeout.h"
#include "platform/assert.h"

__attribute__((weak)) void _exit(int code) { while (1); }

__attribute__((weak)) int _kill(int pid, int signal) {
    errno = ESRCH;
    return -1;
}

__attribute__((weak)) int _close(int file) { return 0; }

__attribute__((weak)) int _fstat(int __fd, struct stat *__sbuf) { return 0; }

__attribute__((weak)) int _isatty(int fd) { return fd == 0 || fd == 1 || fd == 2; }

__attribute__((weak)) off_t _lseek(int fd, off_t ptr, int dir) {
    errno = ESPIPE;
    return -1;
}

__attribute__((weak)) pid_t _getpid(void) { return 1; }

__attribute__((weak)) int _write(int file, char *ptr, int len) { return len; }

__attribute__((weak)) int _read(int file, char *ptr, int len) {
    errno = EBADF;
    return -1;
}

// Weak default for bare-metal (nosys) builds. platform_freertos provides a strong
// override (portYIELD()) that the linker prefers when an app links FreeRTOS instead.
// Calls WFI() unconditionally, regardless of CPU IRQ state - if IRQ is never enabled,
// pending interrupts never get acked, so their line stays asserted forever and every
// future WFI() call (anywhere) returns immediately instead of actually sleeping.
__attribute__((weak)) int sched_yield(void) {
    WFI();
    return 0;
}

// Weak default for bare-metal (nosys) builds; platform_freertos overrides it with vTaskDelay().
// Needs CPU interrupts enabled: the timeout is counted in the timer interrupt.
__attribute__((weak)) int usleep(useconds_t us) {
    if (us == 0) return 0;

    struct timeout_t timeout;
    const bool       created = create_timeout(&timeout, (us + 999) / 1000);
    assert_true(created, "usleep: no free hardware timer");

    while (!is_timeout_finished(&timeout)) sched_yield();
    return 0;
}
