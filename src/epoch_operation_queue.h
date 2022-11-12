#ifndef CACHEGRAND_EPOCH_OPERATION_QUEUE_H
#define CACHEGRAND_EPOCH_OPERATION_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#define EPOCH_OPERATION_QUEUE_RING_SIZE (16 * 1024)

typedef union epoch_operation_queue_operation epoch_operation_queue_operation_t;
union epoch_operation_queue_operation {
    struct {
        bool completed:1;
        uint64_t start_epoch:63;
    } data;
    uint64_t _packed;
};

typedef struct epoch_operation_queue epoch_operation_queue_t;
struct epoch_operation_queue {
    ring_bounded_queue_spsc_uint64_t *ring;
    uint64_t latest_epoch;
};

epoch_operation_queue_t *epoch_operation_queue_init();

void epoch_operation_queue_free(
        epoch_operation_queue_t *epoch_operation_queue);

epoch_operation_queue_operation_t *epoch_operation_queue_enqueue(
        epoch_operation_queue_t *epoch_operation_queue);

void epoch_operation_queue_mark_completed(
        epoch_operation_queue_operation_t *epoch_operation_queue_operation);

uint64_t epoch_operation_queue_get_latest_epoch(
        epoch_operation_queue_t *epoch_operation_queue);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_EPOCH_OPERATION_QUEUE_H
