#ifndef CACHEGRAND_DOUBLE_LINKED_LIST_H
#define CACHEGRAND_DOUBLE_LINKED_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct double_linked_list double_linked_list_t;
typedef struct double_linked_list_item double_linked_list_item_t;

struct double_linked_list_item {
    double_linked_list_t *parent;
    double_linked_list_item_t *prev;
    double_linked_list_item_t *next;
    void* data;
};

struct double_linked_list {
    uint32_t count;
    double_linked_list_item_t *head;
    double_linked_list_item_t *tail;
};

double_linked_list_item_t* double_linked_list_item_init();

void double_linked_list_item_free(
        double_linked_list_item_t *item);

double_linked_list_t* double_linked_list_init();

void double_linked_list_free(
        double_linked_list_t *list);

void double_linked_list_insert_item_before(
        double_linked_list_t *list,
        double_linked_list_item_t *item,
        double_linked_list_item_t *before_item);

void double_linked_list_insert_item_after(
        double_linked_list_t *list,
        double_linked_list_item_t *item,
        double_linked_list_item_t *after_item);

void double_linked_list_push_item(
        double_linked_list_t *list,
        double_linked_list_item_t *item);

double_linked_list_item_t * double_linked_list_pop_item(
        double_linked_list_t *list);

void double_linked_list_unshift_item(
        double_linked_list_t *list,
        double_linked_list_item_t *item);

double_linked_list_item_t * double_linked_list_shift_item(
        double_linked_list_t *list);

void double_linked_list_remove_item(
        double_linked_list_t *list,
        double_linked_list_item_t *item);

void double_linked_list_move_item_to_head(
        double_linked_list_t *list,
        double_linked_list_item_t *item);

void double_linked_list_move_item_to_tail(
        double_linked_list_t *list,
        double_linked_list_item_t *item);

double_linked_list_item_t *double_linked_list_iter_next(
        double_linked_list_t *list,
        double_linked_list_item_t *current_item);

double_linked_list_item_t *double_linked_list_iter_prev(
        double_linked_list_t *list,
        double_linked_list_item_t *current_item);

#define DOUBLE_LINKED_LIST_ITER_FORWARD(list, item, ...) { \
    double_linked_list_item_t *item = list->head; \
    while(item != NULL) { \
        __VA_ARGS__ \
        item = item->next; \
    } \
}

#define DOUBLE_LINKED_LIST_ITER_BACKWARD(list, item, ...) { \
    double_linked_list_item_t *item = list->tail; \
    while(item != NULL) { \
        __VA_ARGS__ \
        item = item->prev; \
    } \
}

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_DOUBLE_LINKED_LIST_H
