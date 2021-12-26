#ifndef CACHEGRAND_FIBER_H
#define CACHEGRAND_FIBER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fiber fiber_t;
typedef void (fiber_start_fp_t)(fiber_t* fiber_from, fiber_t* fiber_to);

struct fiber {
    // The context has to be held at the beginning of the structure and the field have to be specifically kept in this
    // order as the assembly code in fiber.s access these fields by memory location and doesn't rely on the compiler
    // to calculate the correct address.
    // It is possible to implement build workarounds to get a mapping between the fields and their offsets but it is
    // fairly clunky and as this code is not going to change (most likely) it's not worth to pollute the build system
    // just for this.
    // For reference, here a few links describing the workaround
    // https://www.avrfreaks.net/forum/c-assembly-access-structure-fields-assembly
    // https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/arch/arm/kernel/asm-offsets.c
    // https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/include/linux/kbuild.h
    // https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Kbuild
    struct {
        void *rip, *rsp;
        void *rbx, *rbp, *r12, *r13, *r14, *r15;
    } context;
    void* stack_pointer;
    void* stack_base;
    size_t stack_size;
    fiber_start_fp_t* start_fp;
    void* start_fp_user_data;
    char *name;
    union {
        void* ptr_value;
        int int_value;
        long long_value;
        bool bool_value;
        size_t size_value;
        uint32_t uint32_value;
        uint64_t uint64_value;
        int32_t int32_value;
        int64_t int64_value;
    } ret;
} __attribute__((aligned(64)));

extern void fiber_context_get(
        fiber_t *fiber_context);

extern void fiber_context_set(
        fiber_t *fiber_context);

extern void fiber_context_swap(
        fiber_t *fiber_context_from,
        fiber_t *fiber_context_to);

void fiber_stack_protection(
        fiber_t* fiber,
        bool enable);

fiber_t* fiber_new(
        char *name,
        size_t name_len,
        size_t stack_size,
        fiber_start_fp_t* fiber_start_fp,
        void* user_data);

void fiber_free(
        fiber_t* fiber);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_FIBER_H
