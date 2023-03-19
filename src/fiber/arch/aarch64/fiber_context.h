#ifndef CACHEGRAND_FIBER_CONTEXT_H
#define CACHEGRAND_FIBER_CONTEXT_H

typedef struct fiber_context fiber_context_t;

#if __ARM_PCS_VFP
vpush {d8-d15}
    #define FIBER_CONTEXT_NUM_REGISTRIES (9 + 8 * 2)
#else
#define FIBER_CONTEXT_NUM_REGISTRIES 9
#endif

#define FIBER_CONTEXT_NUM_REGISTRIES 6

#endif //CACHEGRAND_FIBER_CONTEXT_H
