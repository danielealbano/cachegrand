#ifndef CACHEGRAND_RING_BOUNDED_SPSC_H
#define CACHEGRAND_RING_BOUNDED_SPSC_H

#ifdef __cplusplus
extern "C" {
#endif

typedef union ring_bounded_mpmc_header ring_bounded_mpmc_header_t;
union ring_bounded_mpmc_header
{
    uint64_t _id;
    struct {
        int16_t maxsize;
        int16_volatile_t head;
        int16_volatile_t tail;
        int16_volatile_t count;
    } data;
};

typedef struct ring_bounded_mpmc ring_bounded_mpmc_t;
struct ring_bounded_mpmc
{
    ring_bounded_mpmc_header_t header;
    void **items;
};

ring_bounded_mpmc_t* ring_bounded_mpmc_init(
        int16_t length);

void ring_bounded_mpmc_free(
        ring_bounded_mpmc_t *scq);

int16_t ring_bounded_mpmc_count(
        ring_bounded_mpmc_t *scq);

bool ring_bounded_mpmc_is_empty(
        ring_bounded_mpmc_t *scq);

bool ring_bounded_mpmc_is_full(
        ring_bounded_mpmc_t *scq);

void *ring_bounded_mpmc_peek(
        ring_bounded_mpmc_t *scq);

bool ring_bounded_mpmc_enqueue(
        ring_bounded_mpmc_t *scq,
        void *value);

void *ring_bounded_mpmc_dequeue(
        ring_bounded_mpmc_t *scq);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_RING_BOUNDED_SPSC_H
