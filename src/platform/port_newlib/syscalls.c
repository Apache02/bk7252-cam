#include <sys/types.h>
#include <sys/stat.h>

void _exit(int code) { while (1); }

int _kill(int pid, int signal) { return -1; }

int _close(int file) { return 0; }

int _fstat(int __fd, struct stat *__sbuf) {
    return 0;
}

int _isatty(int fd) {
    return fd == 0 || fd == 1 || fd == 2;
}

off_t _lseek(int fd, off_t ptr, int dir) {
    return -1;
}

pid_t _getpid(void) {
    return 1;
}
