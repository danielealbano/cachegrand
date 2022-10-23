#ifndef CACHEGRAND_FIBER_CONTEXT_H
#define CACHEGRAND_FIBER_CONTEXT_H

typedef struct fiber_context fiber_context_t;

struct fiber_context {
    char ragisters[176];
    void *sp;
};

#endif //CACHEGRAND_FIBER_CONTEXT_H
