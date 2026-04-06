#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>


__attribute__((weak)) void _exit(int code) {
    while (1);
}

__attribute__((weak)) int _kill(int pid, int signal) {
    errno = ESRCH;
    return -1;
}

__attribute__((weak)) int _close(int file) {
    return 0;
}

__attribute__((weak)) int _fstat(int __fd, struct stat *__sbuf) {
    return 0;
}

__attribute__((weak)) int _isatty(int fd) {
    return fd == 0 || fd == 1 || fd == 2;
}

__attribute__((weak)) off_t _lseek(int fd, off_t ptr, int dir) {
    errno = ESPIPE;
    return -1;
}

__attribute__((weak)) pid_t _getpid(void) {
    return 1;
}

__attribute__((weak)) int _write(int file, char *ptr, int len) {
    return len;
}

__attribute__((weak)) int _read(int file, char *ptr, int len) {
    errno = EBADF;
    return -1;
}
