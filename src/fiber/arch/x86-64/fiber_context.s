/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
*/

.type fiber_context_swap, @function
.global fiber_context_swap
fiber_context_swap:
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, (%rdi)
    movq (%rsi), %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    popq %rcx
    jmpq *%rcx

/**
 * The following section is added as the functions here don't require an executable stack, it's used to fix the
 * following warning:
 * /usr/bin/ld: warning: fiber_context.s.o: missing .note.GNU-stack section implies executable stack
 * /usr/bin/ld: NOTE: This behaviour is deprecated and will be removed in a future version of the linker
 **/
.section .note.GNU-stack,"",%progbits
