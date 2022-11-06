#ifndef CACHEGRAND_RING_BOUNDED_SPSC_H
#define CACHEGRAND_RING_BOUNDED_SPSC_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ring_bounded_spsc ring_bounded_spsc_t;
struct ring_bounded_spsc
{
    uint32_t size;
    uint32_t mask;
    uint32_volatile_t head;
    uint32_volatile_t tail;
    volatile void **items;
};

ring_bounded_spsc_t* ring_bounded_spsc_init(
        uint32_t size);

void ring_bounded_spsc_free(
        ring_bounded_spsc_t *rb);

uint32_t ring_bounded_spsc_get_length(
        ring_bounded_spsc_t *rb);

bool ring_bounded_spsc_is_empty(
        ring_bounded_spsc_t *rb);

bool ring_bounded_spsc_is_full(
        ring_bounded_spsc_t *rb);

void *ring_bounded_spsc_peek(
        ring_bounded_spsc_t *rb);

bool ring_bounded_spsc_enqueue(
        ring_bounded_spsc_t *rb,
        void *value);

void *ring_bounded_spsc_dequeue(
        ring_bounded_spsc_t *rb);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_RING_BOUNDED_SPSC_H
