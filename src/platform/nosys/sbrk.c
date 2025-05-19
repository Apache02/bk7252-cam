#include <sys/reent.h>
#include <errno.h>


extern char end;
extern char _stack_unused;

static char *heap_end = &end;

void *_sbrk(ptrdiff_t incr) {
    char *prev_heap_end = heap_end;

    if (heap_end + incr > &_stack_unused) {
        errno = ENOMEM;
        return (void *) -1;
    }

    heap_end += incr;
    return (void *) prev_heap_end;
}
