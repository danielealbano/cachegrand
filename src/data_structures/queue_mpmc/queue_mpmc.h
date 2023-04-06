#ifndef CACHEGRAND_QUEUE_MPMC_H
#define CACHEGRAND_QUEUE_MPMC_H

// For optimization purposes a number of functions are in this header as static inline and they need certain headers,
// so we need to include them here to avoid having to include them in every file that includes this header.
#ifdef __cplusplus
#include <atomic>
#else
#include <stdatomic.h>
#endif
#include "memory_fences.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t queue_mpmc_node_t;
typedef _Volatile(queue_mpmc_node_t) queue_mpmc_node_volatile_t;

typedef struct queue_mpmc_page queue_mpmc_page_t;
typedef _Volatile(queue_mpmc_page_t) queue_mpmc_page_volatile_t;
struct queue_mpmc_page {
    queue_mpmc_page_volatile_t *prev_page;
    queue_mpmc_page_volatile_t *next_page;
    queue_mpmc_node_volatile_t nodes[];
};

typedef struct queue_mpmc_versioned_head queue_mpmc_versioned_head_t;
struct queue_mpmc_versioned_head {
    union {
        uint128_t _packed;
        struct {
            // The length, the node_index and the nodes_page fields are accessed both using atomic ops and memory
            // fences, therefore they both need to be volatile.
            // The field version is used only internally to avoid the A-B-A problem and is only used via atomic ops.
            uint32_volatile_t length;
            uint16_volatile_t version;
            int16_volatile_t node_index;
            queue_mpmc_page_volatile_t *nodes_page;
        } data;
    };
};

typedef struct queue_mpmc queue_mpmc_t;
struct queue_mpmc {
    queue_mpmc_versioned_head_t head;
    int16_t max_nodes_per_page;
};

queue_mpmc_t *queue_mpmc_init();

void queue_mpmc_free(queue_mpmc_t *queue_mpmc);

bool queue_mpmc_push(
        queue_mpmc_t *queue_mpmc,
        void *data);

void *queue_mpmc_pop(
        queue_mpmc_t *queue_mpmc);

static inline uint32_t queue_mpmc_get_length(
        queue_mpmc_t *queue_mpmc) {
    MEMORY_FENCE_LOAD();
    return (uint32_t)queue_mpmc->head.data.length;
}

static inline bool queue_mpmc_is_empty(
        queue_mpmc_t *queue_mpmc) {
    return queue_mpmc_get_length(queue_mpmc) == 0;
}


#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_QUEUE_MPMC_H
