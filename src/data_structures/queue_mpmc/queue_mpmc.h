#ifndef CACHEGRAND_QUEUE_MPMC_H
#define CACHEGRAND_QUEUE_MPMC_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct queue_mpmc_node queue_mpmc_node_t;
typedef _Volatile(queue_mpmc_node_t) queue_mpmc_node_volatile_t;
struct queue_mpmc_node {
    void *data;
    queue_mpmc_node_t *next;
};

typedef struct queue_mpmc_versioned_head queue_mpmc_versioned_head_t;
struct queue_mpmc_versioned_head {
    union {
        uint128_t _packed;
        struct {
            // The length and the node fields are accessed both using atomic ops and not, syncing the data using
            // memory fences where needed, therefore they both need to be volatile, version is used only internally
            // to avoid the A-B-A problem and is only used via atomic ops.
            uint32_volatile_t length;
            uint16_t version;
            queue_mpmc_node_volatile_t *node;
        } data;
    };
};

typedef struct queue_mpmc queue_mpmc_t;
struct queue_mpmc {
    queue_mpmc_versioned_head_t head;
};


queue_mpmc_t *queue_mpmc_init();

void queue_mpmc_free(queue_mpmc_t *queue_mpmc);

void queue_mpmc_push(
        queue_mpmc_t *queue_mpmc,
        void *data);

void *queue_mpmc_pop(
        queue_mpmc_t *queue_mpmc);

uint32_t queue_mpmc_get_length(
        queue_mpmc_t *queue_mpmc);

queue_mpmc_node_t *queue_mpmc_peek(
        queue_mpmc_t *queue_mpmc);

bool queue_mpmc_is_empty(
        queue_mpmc_t *queue_mpmc);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_QUEUE_MPMC_H
