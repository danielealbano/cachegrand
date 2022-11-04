#ifndef CACHEGRAND_RING_BOUNDED_SPSC_H
#define CACHEGRAND_RING_BOUNDED_SPSC_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ring_bounded_mpmc ring_bounded_mpmc_t;
struct ring_bounded_mpmc
{
    int16_t size;
    int16_t mask;
    uint64_t head;
    uint64_t tail;
    void **items;
};

ring_bounded_mpmc_t* ring_bounded_mpmc_init(
        int16_t size);

void ring_bounded_mpmc_free(
        ring_bounded_mpmc_t *rb);

bool ring_bounded_mpmc_enqueue(
        ring_bounded_mpmc_t *rb,
        void *value);

void *ring_bounded_mpmc_dequeue(
        ring_bounded_mpmc_t *rb);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_RING_BOUNDED_SPSC_H
