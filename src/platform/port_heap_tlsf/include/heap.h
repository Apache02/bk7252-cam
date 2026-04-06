#ifndef HEAP_H
#define HEAP_H


// TODO: add slow RAM region
// enum {
//     HEAP_FAST = 0,
//     HEAP_SLOW,
//     _HEAP_COUNT,
// };

typedef struct {
    size_t used_bytes;
    size_t used_blocks;
    size_t free_bytes;
    size_t free_blocks;
    size_t largest_free;
} heap_statistics_t;


#ifdef __cplusplus
extern "C" {
#endif


void heap_init(void);

heap_statistics_t heap_statistics(void);


#ifdef __cplusplus
}
#endif

#endif // HEAP_H
