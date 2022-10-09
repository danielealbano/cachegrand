#ifndef CACHEGRAND_FIBER_CONTEXT_H
#define CACHEGRAND_FIBER_CONTEXT_H

typedef struct fiber_context fiber_context_t;

struct fiber_context {
    void *sp;
    void *d8, *d9, *d10, *d11, *d12, *d13, *d14, *d15;
    void *x19, *x20, *x21, *x22, *x23, *x24, *x25, *x26, *x27, *x28, *x29, *x30;
};

#endif //CACHEGRAND_FIBER_CONTEXT_H
