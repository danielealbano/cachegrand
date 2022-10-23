#ifndef CACHEGRAND_FIBER_CONTEXT_H
#define CACHEGRAND_FIBER_CONTEXT_H

typedef struct fiber_context fiber_context_t;

struct fiber_context {
    void *rip, *rsp;
    void *rbx, *rbp, *r12, *r13, *r14, *r15;
};

#endif //CACHEGRAND_FIBER_CONTEXT_H
