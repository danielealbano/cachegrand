#ifndef CACHEGRAND_FIBER_SCHEDULER_H
#define CACHEGRAND_FIBER_SCHEDULER_H

#ifdef __cplusplus
extern "C" {
#endif

#define FIBER_SCHEDULER_STACK_SIZE (4096 * 5)
#define FIBER_SCHEDULER_STACK_MAX_SIZE 5
#define FIBER_SCHEDULER_FIBER_NAME "scheduler"

typedef void (fiber_scheduler_entrypoint_fp_t)(void *user_data);

typedef struct fiber_scheduler_stack fiber_scheduler_stack_t;
struct fiber_scheduler_stack {
    fiber_t **stack;
    int8_t index;
    int8_t size;
};

typedef struct fiber_scheduler_new_fiber_user_data fiber_scheduler_new_fiber_user_data_t;
struct fiber_scheduler_new_fiber_user_data {
    fiber_scheduler_entrypoint_fp_t* caller_entrypoint_fp;
    void *caller_user_data;
};

void fiber_scheduler_grow_stack();

bool fiber_scheduler_stack_needs_growth();

void fiber_scheduler_new_fiber_entrypoint(
        fiber_t *from,
        fiber_t *to);

fiber_t* fiber_scheduler_new_fiber(
        char *name,
        size_t name_len,
        fiber_scheduler_entrypoint_fp_t* entrypoint_fp,
        void *user_data);

void fiber_scheduler_switch_to(
        fiber_t *fiber);

#if DEBUG == 1
void fiber_scheduler_switch_back_internal(
        const char *file,
        int line,
        const char *func);
#define fiber_scheduler_switch_back() fiber_scheduler_switch_back_internal(CACHEGRAND_SRC_PATH, __LINE__, __func__)
#else
void fiber_scheduler_switch_back();
#endif

fiber_t *fiber_scheduler_get_current();

void fiber_scheduler_set_error(
        int error_number);

int fiber_scheduler_get_error();

bool fiber_scheduler_has_error();

void fiber_scheduler_reset_error();

void fiber_scheduler_terminate_current_fiber();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_FIBER_SCHEDULER_H
