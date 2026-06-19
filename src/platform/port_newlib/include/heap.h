#ifndef HEAP_H
#define HEAP_H

typedef struct {
    size_t used;
    size_t total;
} heap_stat_t;

#ifdef __cplusplus
extern "C" {
#endif

void heap_init(void);

heap_stat_t newlib_heap_get_stat();

#ifdef __cplusplus
}
#endif

#endif // HEAP_H
