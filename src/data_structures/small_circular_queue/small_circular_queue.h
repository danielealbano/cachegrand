#ifndef CACHEGRAND_SMALL_CIRCULAR_QUEUE_H
#define CACHEGRAND_SMALL_CIRCULAR_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct small_circular_queue small_circular_queue_t;
struct small_circular_queue
{
    int16_t maxsize;
    int16_t head;
    int16_t tail;
    int16_t count;
    void **items;
};

small_circular_queue_t* small_circular_queue_init(
        int16_t length);

void small_circular_queue_free(
        small_circular_queue_t *scq);

int16_t small_circular_queue_count(
        small_circular_queue_t *scq);

bool small_circular_queue_is_empty(
        small_circular_queue_t *scq);

bool small_circular_queue_is_full(
        small_circular_queue_t *scq);

void *small_circular_queue_peek(
        small_circular_queue_t *scq);

bool small_circular_queue_enqueue(
        small_circular_queue_t *scq,
        void *value);

void *small_circular_queue_dequeue(
        small_circular_queue_t *scq);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_SMALL_CIRCULAR_QUEUE_H
