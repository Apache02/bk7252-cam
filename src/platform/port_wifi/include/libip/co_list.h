#pragma once

// ip/common/co_list.h — the archive's singly-linked list, the container every
// kernel queue is built from. Layouts and signatures are dictated by
// libip_7221u.a (co_list.o) and were read out of its own DWARF.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Embedded as the first member of whatever is being linked, so a node pointer
// and its element pointer are the same address.
struct co_list_hdr {
    struct co_list_hdr *next;
};

struct co_list {
    struct co_list_hdr *first;
    struct co_list_hdr *last;
};

void                co_list_init(struct co_list *list);
void                co_list_pool_init(struct co_list *list, void *pool, size_t elmt_size, uint32_t elmt_cnt,
                                      void *default_value);
void                co_list_push_back(struct co_list *list, struct co_list_hdr *element);
void                co_list_push_front(struct co_list *list, struct co_list_hdr *element);
struct co_list_hdr *co_list_pop_front(struct co_list *list);
void                co_list_extract(struct co_list *list, struct co_list_hdr *element);
bool                co_list_find(struct co_list *list, struct co_list_hdr *element);
void                co_list_insert(struct co_list *const list, struct co_list_hdr *const element,
                                   bool (*cmp)(struct co_list_hdr const *a, struct co_list_hdr const *b));
void                co_list_insert_after(struct co_list *const list, struct co_list_hdr *const prev_element,
                                         struct co_list_hdr *const element);
void                co_list_insert_before(struct co_list *const list, struct co_list_hdr *const next_element,
                                          struct co_list_hdr *const element);
void                co_list_concat(struct co_list *list1, struct co_list *list2);
uint32_t            co_list_cnt(const struct co_list *const list);

// Inline in the vendor header, so the archive exports no symbol for these.
static inline bool                co_list_is_empty(const struct co_list *const list) { return list->first == NULL; }
static inline struct co_list_hdr *co_list_pick(const struct co_list *const list) { return list->first; }
static inline struct co_list_hdr *co_list_pick_last(const struct co_list *const list) { return list->last; }
static inline struct co_list_hdr *co_list_next(const struct co_list_hdr *const element) { return element->next; }

#ifdef __cplusplus
}
#endif
