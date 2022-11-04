#ifndef CACHEGRAND_RING_BOUNDED_SPSC_H
#define CACHEGRAND_RING_BOUNDED_SPSC_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ring_bounded_spsc ring_bounded_spsc_t;
struct ring_bounded_spsc
{
    int16_t maxsize;
    int16_t head;
    int16_t tail;
    int16_t count;
    void **items;
};

ring_bounded_spsc_t* ring_bounded_spsc_init(
        int16_t length);

void ring_bounded_spsc_free(
        ring_bounded_spsc_t *rb);

int16_t ring_bounded_spsc_count(
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
