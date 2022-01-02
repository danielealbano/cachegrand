#ifndef CACHEGRAND_FIBER_SCHEDULER_H
#define CACHEGRAND_FIBER_SCHEDULER_H

#ifdef __cplusplus
extern "C" {
#endif

#define FIBER_SCHEDULER_STACK_SIZE (4096 * 5)

typedef void (fiber_scheduler_entrypoint_fp_t)(void *user_data);

fiber_t* fiber_scheduler_new_fiber(
        char *name,
        size_t name_len,
        fiber_scheduler_entrypoint_fp_t* entrypoint_fp,
        void *user_data);

void fiber_scheduler_switch_to(fiber_t *fiber);

#if DEBUG == 1
void fiber_scheduler_switch_back_internal(
        char *file,
        int line,
        const char *func);
#define fiber_scheduler_switch_back() fiber_scheduler_switch_back_internal(CACHEGRAND_SRC_PATH, __LINE__, __func__)
#else
void fiber_scheduler_switch_back();
#endif

fiber_t *fiber_scheduler_get_current();

void fiber_scheduler_terminate_current_fiber();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_FIBER_SCHEDULER_H
